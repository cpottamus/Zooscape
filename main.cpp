/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
#include "assets.gen.h"
using namespace Sifteo;

// METADATA

static Metadata M = Metadata()
    .title("Pairing Example")
    .package("com.sifteo.sdk.pairing", "0.1")
    .icon(Icon)
    .cubeRange(1, CUBE_ALLOCATION);


AssetSlot gMainSlot = AssetSlot::allocate()
    .bootstrap(BootstrapAssets);

// GLOBALS

static VideoBuffer vbuf[CUBE_ALLOCATION]; // one video-buffer per cube
static TiltShakeRecognizer motion[CUBE_ALLOCATION]; //for shaking!
static CubeSet newCubes; // new cubes as a result of paint()
static CubeSet lostCubes; // lost cubes as a result of paint()
static CubeSet reconnectedCubes; // reconnected (lost->new) cubes as a result of paint()
static CubeSet dirtyCubes; // dirty cubes as a result of paint()
static CubeSet activeCubes; // cubes showing the active scene

static AssetLoader loader; // global asset loader (each cube will have symmetric assets)
static AssetConfiguration<1> config; // global asset configuration (will just hold the bootstrap group)


//Cube globals
// static int gNumCubes = 3;
static CubeID cbs [3];
int currentBackgrounds [3];

//Event Globals
int currentLearningTask;
int previousLearningTask;

//Character globals
    //LT 1
static CubeID toucan;
static CubeID monkey;
static CubeID giraffe;
    //LT 2
static CubeID zookeeper;
    //LT 3
static CubeID chameleon;
int chamColor;
static CubeID yellowFlower;
static CubeID purpleFlower;
    //LT 4
static CubeID penguin;
static CubeID ice1;
static CubeID ice2;
    //LT 5
static CubeID kangaroo;
static CubeID joey;
static CubeID trampoline;

//Time Globals
SystemTime distractTime;
bool isDistracted;

// FUNCTIONS
//Learning task function declarations
static void learningTask1();
static void lt1SideCheck(unsigned cube0, unsigned cube1);
static void learningTask2();
static void lt2SideCheck(unsigned cube0, unsigned cube1);
static void learningTask3();
static void lt3SideCheck(unsigned cube0, unsigned cube1);
static void learningTask4();
static void lt4SideCheck(unsigned cube0, unsigned cube1);
static void learningTask5();
static void lt5SideCheck(unsigned cube0, unsigned cube1);


static void playSfx(const AssetAudio& sfx) {
    static int i=0;
    AudioChannel(i).play(sfx);
    i = 1 - i;
}

static void onCubeConnect(void* ctxt, unsigned cid) {
    // this cube is either new or reconnected
    if (lostCubes.test(cid)) {
        // this is a reconnected cube since it was already lost this paint()
        lostCubes.clear(cid);
        reconnectedCubes.mark(cid);
    } else {
        // this is a brand-spanking new cube
        newCubes.mark(cid);
    }
    // begin showing some loading art (have to use BG0ROM since we don't have assets)
    dirtyCubes.mark(cid);
    auto& g = vbuf[cid];
    g.attach(cid);
    g.initMode(BG0_ROM);
    g.bg0rom.fill(vec(0,0), vec(16,16), BG0ROMDrawable::SOLID_BG);
    g.bg0rom.text(vec(1,1), "Hold on!", BG0ROMDrawable::BLUE);
    g.bg0rom.text(vec(1,14), "Adding Cube...", BG0ROMDrawable::BLUE);
}

static void onCubeDisconnect(void* ctxt, unsigned cid) {
    // mark as lost and clear from other cube sets
    lostCubes.mark(cid);
    newCubes.clear(cid);
    reconnectedCubes.clear(cid);
    dirtyCubes.clear(cid);
    activeCubes.clear(cid);
}

static void onCubeRefresh(void* ctxt, unsigned cid) {
    // mark this cube for a future repaint
    dirtyCubes.mark(cid);
}


static void activateCube(CubeID cid) {
    // mark cube as active and render its canvas
    activeCubes.mark(cid);
    
    vbuf[cid].initMode(BG0_SPR_BG1);
    vbuf[cid].bg0.image(vec(0,0), Backgrounds, currentBackgrounds[(int)cid]);
    
    //Old sidebar code
    //auto neighbors = vbuf[cid].physicalNeighbors();
    // for(int side=0; side<4; ++side) {
    //     if (neighbors.hasNeighborAt(Side(side))) {
    //         showSideBar(cid, Side(side));
    //     } else {
    //         hideSideBar(cid, Side(side));
    //     }
    // }
}

static void paintWrapper() {
    // clear the palette
    newCubes.clear();
    lostCubes.clear();
    reconnectedCubes.clear();
    dirtyCubes.clear();

        if(previousLearningTask != currentLearningTask){
        previousLearningTask++;
        }


    // fire events
    System::paint();

    // dynamically load assets just-in-time
    if (!(newCubes | reconnectedCubes).empty()) {
        AudioTracker::pause();
        playSfx(SfxConnect);
        loader.start(config);
        while(!loader.isComplete()) {
            for(CubeID cid : (newCubes | reconnectedCubes)) {
                vbuf[cid].bg0rom.hBargraph(
                    vec(0, 4), loader.cubeProgress(cid, 128), BG0ROMDrawable::ORANGE, 8
                );
            }
            // fire events while we wait
            System::paint();
        }
        loader.finish();
        AudioTracker::resume();
    }


    // // repaint cubes (will this paint right? If not, try repainting all of them)
    // for(CubeID cid : dirtyCubes) {
    //     activateCube(cid);
    // }

    //If the shaken timer flag is too old, turn it off again here.
    if(distractTime.isValid() && (currentLearningTask==2)) {

        TimeDelta timeSinceShook = SystemTime::now() - distractTime;
        double duration = timeSinceShook.milliseconds() / 1000;
        if((duration > 11) && isDistracted) {
            currentBackgrounds[2] = 3;
            currentBackgrounds[1] = 1;
            isDistracted = false;
        }
    }

    //update art to new task

    int j = 0;
    for(CubeID cid : CubeSet::connected()) {
        vbuf[cid].attach(cid);
        activateCube(cid);
        cbs [j] = cid;
        j++;
    }


    // also, handle lost cubes, if you so desire :)
}

static bool isActive(NeighborID nid) {
    // Does this nid indicate an active cube?
    return nid.isCube() && activeCubes.test(nid);
}

static void onNeighborAdd(void* ctxt, unsigned cube0, unsigned side0, unsigned cube1, unsigned side1) {
    //Create Art Animation Events based off of collisions between cubes

    //Check for end of game here

    if(currentLearningTask==1){
        lt1SideCheck(cube0, cube1);
        learningTask1();
    } else if(currentLearningTask==2){
        lt2SideCheck(cube0, cube1);
        learningTask2();
    } else if (currentLearningTask==3){
        lt3SideCheck(cube0, cube1);
        learningTask3();
    } else if (currentLearningTask==4){
        lt4SideCheck(cube0, cube1);
        learningTask4();
    } else if (currentLearningTask==5){
        lt5SideCheck(cube0, cube1);
        learningTask5();
    } else {
        //Error Case
        Sifteo::System::exit();
    }

    ///Old Code///
    // update art on active cubes (not loading cubes or base)
    bool sfx = false;
    // if (isActive(cube0)) { sfx |= showSideBar(cube0, Side(side0)); }
    // if (isActive(cube1)) { sfx |= showSideBar(cube1, Side(side1)); }
    // if (sfx) { playSfx(SfxAttach); }

}

static void onNeighborRemove(void* ctxt, unsigned cube0, unsigned side0, unsigned cube1, unsigned side1) {
    // update art on active cubes (not loading cubes or base)
    bool sfx = false;

    if(currentLearningTask==1){
        lt1SideCheck(cube0, cube1);
    } else if(currentLearningTask==2){
        lt2SideCheck(cube0, cube1);
    } else if (currentLearningTask==3){
        lt3SideCheck(cube0, cube1);
    } else if (currentLearningTask==4){
        
    } else if (currentLearningTask==5){
        
    } else {
        //Error Case
        Sifteo::System::exit();
    }
    // if (isActive(cube0)) { sfx |= hideSideBar(cube0, Side(side0)); }
    // if (isActive(cube1)) { sfx |= hideSideBar(cube1, Side(side1)); }
    // if (sfx) { playSfx(SfxDetach); }
}

static void onAccelChange(void* ctxt, unsigned cube0) {
    CubeID cube(cube0);
    auto accel = cube.accel();

    unsigned changeFlags = motion[cube0].update();
    // LOG("CHANGEFLAG ::: %d \n", changeFlags);

    if(changeFlags) {     
        unsigned shakeIt = motion[cube0].shake;

        if(shakeIt){
            //If the shake happens, reset a global timerflag and make it active.
            if((int(cube0) == int(monkey)) && (currentLearningTask == 2)) {
                //Set timer on for distracted zookeeper
                distractTime = SystemTime::now();
                //Update distracted zookeeper art
                currentBackgrounds[2] = 15;
                //Set distracted
                isDistracted = true;


            }
        }
    }
}

void main() {
    // initialize asset configuration and loader
    config.append(gMainSlot, BootstrapAssets);
    loader.init();

    // subscribe to events
    Events::cubeConnect.set(onCubeConnect);
    Events::cubeDisconnect.set(onCubeDisconnect);
    Events::cubeRefresh.set(onCubeRefresh);


    //Set events for interactive elements
    Events::neighborAdd.set(onNeighborAdd);
    Events::neighborRemove.set(onNeighborRemove);
    Events::cubeAccelChange.set(onAccelChange);
    
    // initialize cubes
    currentLearningTask=1;
    previousLearningTask=0;

    isDistracted = false;
    
    // Background music
    // AudioTracker::setVolume(0.2f * AudioChannel::MAX_VOLUME);
    // AudioTracker::play(Music);

    //Initial cube paint, also assigning character cubes. (should retain same IDs)
    int j = 0;
    for(CubeID cid : CubeSet::connected()) {
        currentBackgrounds[j] = j;
        vbuf[cid].attach(cid);
        motion[cid].attach(cid);
        activateCube(cid);
        cbs [j] = cid;
        j++;
    }

    toucan = cbs[0];
    monkey = cbs[1];
    giraffe = cbs[2];

    
    // run loop
    for(;;) {
        paintWrapper();

    }
}


///////Learning Task 1/////////
static void learningTask1(){

    Neighborhood nb1(monkey);
    //Neighborhood nb2(giraffe);

    //Get CubeIDs
    int bottomMonkey = int((nb1.neighborAt(BOTTOM)));
    int leftMonkey = int((nb1.neighborAt(LEFT)));
    int rightMonkey = int((nb1.neighborAt(RIGHT)));
    int topMonkey = int((nb1.neighborAt(TOP)));

    //if monkey on giraffe & touching toucan
    if( (bottomMonkey == int(giraffe)) && (leftMonkey == int(toucan) || topMonkey == int(toucan) || rightMonkey == int(toucan)))
    {
        //Create Success Response, move on.
        LOG(":::::: You Passed Level 1 ::::::\nGREAT SUCCESS\n\n\n\n");
        LOG(":::::::: Beginning Learning Task 2:::::::::\n");
        //increment current task
        currentLearningTask++;
        //Update the background images to reflect new cube backgrounds
        currentBackgrounds[0] = 0;
        currentBackgrounds[1] = 1;
        currentBackgrounds[2] = 3;
        zookeeper = cbs[2];
        paintWrapper();

    }
}


//Called on neighbor add and remove, reviews state of cubes, adjust sprite graphics accordingly
static void lt1SideCheck(unsigned cube0, unsigned cube1){

    int monkInt = int(monkey);
    int girInt = int(giraffe);
    int toucInt = int(toucan);

    int c0 = int(cube0);
    int c1 = int(cube1);
    


    //If monkey was effected
    if(c0 == monkInt || c1 == monkInt) {
        LOG("MONKEY EFFECTED\n");
        Neighborhood nb1(monkey);

        int bottomMonkey = int((nb1.neighborAt(BOTTOM)));
        int leftMonkey = int((nb1.neighborAt(LEFT)));
        int rightMonkey = int((nb1.neighborAt(RIGHT)));
        int topMonkey = int((nb1.neighborAt(TOP)));

        if (((leftMonkey == girInt) || (rightMonkey == girInt) || (topMonkey == girInt)) || (bottomMonkey == toucInt)) {
            //wrong monkey
            currentBackgrounds[1] = 4;

            //audio: Negative reinforcement.

        } else if( ((leftMonkey == toucInt) || (topMonkey == toucInt) || (rightMonkey == toucInt))
                    || (bottomMonkey == girInt) ) {
            //monkey partially right
            currentBackgrounds[1] = 12;

            //audio: Positive reinforcement
        } else {
            //monkey idle
            currentBackgrounds[1] = 1;
        }

    } 
    //if toucan was effected
    if(c0 == toucInt || c1 == toucInt){
        LOG("TOUCAN EFFECTED\n");
        Neighborhood nb2(toucan);

        int bToucan = int((nb2.neighborAt(BOTTOM)));
        int lToucan = int((nb2.neighborAt(LEFT)));
        int rToucan = int((nb2.neighborAt(RIGHT)));
        int tToucan = int((nb2.neighborAt(TOP)));

        if(((bToucan == girInt) || (lToucan == girInt) || (rToucan == girInt) || (tToucan == girInt))
            || (tToucan == monkInt)){
            //wrong toucan
            currentBackgrounds[0] = 9;
        } else if((bToucan == monkInt) || (rToucan == monkInt) || (lToucan == monkInt)){
            //Right toucan
            currentBackgrounds[0] = 13;
        }else{
            //Idle toucan
            currentBackgrounds[0] = 0;
        }

    }
    //If giraffe was effected
    if(c0 == girInt || c1 == girInt) {
        LOG("GIRAFFE EFFECTED\n");

        Neighborhood nb3(giraffe);

        int bGiraffe = int((nb3.neighborAt(BOTTOM)));
        int lGiraffe = int((nb3.neighborAt(LEFT)));
        int rGiraffe = int((nb3.neighborAt(RIGHT)));
        int tGiraffe = int((nb3.neighborAt(TOP)));

        if((tGiraffe == toucInt) || (lGiraffe == toucInt) || (rGiraffe == toucInt) || (bGiraffe == toucInt) ||
            (lGiraffe == monkInt) || (rGiraffe == monkInt) || (bGiraffe == monkInt)){
            //Wrong Giraffe
            currentBackgrounds[2] = 6;
        } else if(tGiraffe == monkInt) {
            //Right Giraffe
            currentBackgrounds[2] = 14;
        } else {
            //Idle Giraffe
            currentBackgrounds[2] = 2;
        }

    }

    paintWrapper();
}


///////Learning Task 2/////////


static void learningTask2(){

    Neighborhood nb(zookeeper);

    int bZookeeper = int((nb.neighborAt(BOTTOM)));
    int lZookeeper = int((nb.neighborAt(LEFT)));
    int rZookeeper = int((nb.neighborAt(RIGHT)));
    int tZookeeper = int((nb.neighborAt(TOP)));

    int toucInt = int(toucan);
    //if zookeeper is distracted & zookeeper/toucan are touching.
    if(isDistracted && ((bZookeeper == toucInt) || (lZookeeper == toucInt) || (rZookeeper == toucInt) || (tZookeeper == toucInt)))
    {
        //Create Success Response, end game.
        LOG(":::::: You Passed Level 2 ::::::\nGREAT SUCCESS\n\n\n\n");
        LOG(":::::::: Beginning Learning Task 3:::::::::\n");
        currentLearningTask++;
        isDistracted == false;
        //Chameleon
        currentBackgrounds[0] = 16;
        chameleon = cbs[0];
        chamColor = 0;
        //purple flower
        currentBackgrounds[1] = 17;
        purpleFlower = cbs[1];
        //yellow flower
        currentBackgrounds[2] = 18;
        yellowFlower = cbs[2];


    }
}

static void lt2SideCheck(unsigned cube0, unsigned cube1){

    int monkInt = int(monkey);
    int zookInt = int(zookeeper);
    int toucInt = int(toucan);

    int c0 = int(cube0);
    int c1 = int(cube1);

        //If monkey was effected
    if(c0 == monkInt || c1 == monkInt) {
        LOG("MONKEY EFFECTED\n");
        Neighborhood nb1(monkey);

        int bottomMonkey = int((nb1.neighborAt(BOTTOM)));
        int leftMonkey = int((nb1.neighborAt(LEFT)));
        int rightMonkey = int((nb1.neighborAt(RIGHT)));
        int topMonkey = int((nb1.neighborAt(TOP)));

        if (((leftMonkey == toucInt) || (rightMonkey == toucInt) || (topMonkey == toucInt) || (bottomMonkey == toucInt))
            || ((leftMonkey == zookInt) || (rightMonkey == zookInt) || (topMonkey == zookInt) || (bottomMonkey == zookInt))) {
            //wrong monkey
            currentBackgrounds[1] = 4;

            //audio: Negative reinforcement.
            //"Shake me!"
        } else if( isDistracted) {
            //monkey partially right
            currentBackgrounds[1] = 12;

            //audio: Positive reinforcement
        } else {
            //monkey idle
            currentBackgrounds[1] = 1;
        }

    }

    if(c0 == toucInt || c1 == toucInt){
        LOG("TOUCAN EFFECTED\n");
        Neighborhood nb2(toucan);

        int bToucan = int((nb2.neighborAt(BOTTOM)));
        int lToucan = int((nb2.neighborAt(LEFT)));
        int rToucan = int((nb2.neighborAt(RIGHT)));
        int tToucan = int((nb2.neighborAt(TOP)));

        if((((bToucan == zookInt) || (lToucan == zookInt) || (rToucan == zookInt) || (tToucan == zookInt)) && !isDistracted)
            || ((bToucan == monkInt) || (lToucan == monkInt) || (rToucan == monkInt) || (tToucan == monkInt))){
            //wrong toucan
            currentBackgrounds[0] = 9;
        }else{
            //Idle toucan
            currentBackgrounds[0] = 0;
        }

    } 

        //If zookeeper was effected
    if(c0 == zookInt || c1 == zookInt) {
        LOG("GIRAFFE EFFECTED\n");

        Neighborhood nb3(zookeeper);

        int bZookeeper = int((nb3.neighborAt(BOTTOM)));
        int lZookeeper = int((nb3.neighborAt(LEFT)));
        int rZookeeper = int((nb3.neighborAt(RIGHT)));
        int tZookeeper = int((nb3.neighborAt(TOP)));

        if(((tZookeeper == monkInt) || (lZookeeper == monkInt) || (rZookeeper == monkInt) || (bZookeeper == monkInt)) ||
            (((lZookeeper == toucInt) || (rZookeeper == toucInt) || (bZookeeper == toucInt) || (tZookeeper == toucInt)) && !isDistracted)){
            //Wrong Zookeeper
            currentBackgrounds[2] = 11;
        } else {
            //Idle Zookeeper
            if(isDistracted){
                currentBackgrounds[2] = 15;
            } else {
                currentBackgrounds[2] = 3;
            }
        }

    }
    
}

static void learningTask3(){

    //If chameleon moves in the right order. Yellow then purple.
    if(currentBackgrounds[0] == 22)
    {
        //Create Success Response, end game.
        LOG(":::::: You Passed Level 3 ::::::\nGREAT SUCCESS\n\n\n\n");
        LOG(":::::::: Beginning Learning Task 4:::::::::\n");
        currentLearningTask++;
        currentBackgrounds[0] = 23;
        penguin = cbs[0];
        currentBackgrounds[1] = 24;
        ice1 = cbs[1];
        currentBackgrounds[2] = 25;
        ice2 = cbs[2];

    } //Add further conditionals to reinforce negative feedback
    //else if (monkey touching giraffe not on top)
        //Then animate monkey and giraffe negatively
    //else if (giraffe touching toucan)
        //Then animate toucan and giraffe negatively
}

static void lt3SideCheck(unsigned cube0, unsigned cube1){

    //if green
        //if touch purple, wrong
        //if touch yellow, turn yellow
    //if yellow
        //if touch yellow, wrong
        //if touch purple, turn purple

    int chamInt = int(chameleon);
    int yellInt = int(yellowFlower);
    int purpInt = int(purpleFlower);

    int c0 = int(cube0);
    int c1 = int(cube1);







}

static void learningTask4(){

    //If Penguin, then Ice 1, then Ice 2

    Neighborhood nb(ice1);

    int bIce1 = int((nb.neighborAt(BOTTOM)));
    int lIce1 = int((nb.neighborAt(LEFT)));
    int rIce1 = int((nb.neighborAt(RIGHT)));
    int tIce1 = int((nb.neighborAt(TOP)));


    if(lIce1 == int(penguin) && rIce1 == int(ice2))
    {
        //Create Success Response, end game.
        LOG(":::::: You Passed Level 4 ::::::\nGREAT SUCCESS\n\n\n\n");
        LOG(":::::::: Beginning Learning Task 5:::::::::\n");
        currentLearningTask++;
        kangaroo = cbs[0];
        currentBackgrounds[0] = 26;

        joey = cbs[1];
        currentBackgrounds[1] = 28;

        trampoline = cbs[2];
        currentBackgrounds[2] = 30;
    } //Add further conditionals to reinforce negative feedback
    //else if (monkey touching giraffe not on top)
        //Then animate monkey and giraffe negatively
    //else if (giraffe touching toucan)
        //Then animate toucan and giraffe negatively
}

static void lt4SideCheck(unsigned cube0, unsigned cube1){
    



    int pengInt = int(penguin);
    int ice1Int = int(ice1);
    int ice2Int = int(ice2);

    int c0 = int(cube0);
    int c1 = int(cube1);


    //If penguin
    if(c0 == pengInt || c1 == pengInt) {
        LOG("Penguin EFFECTED\n");
        Neighborhood nb1(penguin);

        int bottomPenguin = int((nb1.neighborAt(BOTTOM)));
        int leftPenguin = int((nb1.neighborAt(LEFT)));
        int rightPenguin = int((nb1.neighborAt(RIGHT)));
        int topPenguin = int((nb1.neighborAt(TOP)));

        if (rightPenguin == ice1Int && !nb1.hasNeighborAt(BOTTOM) && !nb1.hasNeighborAt(LEFT) && !nb1.hasNeighborAt(TOP)) {
            //right Penguin

            currentBackgrounds[0] = 34;

            //audio: correct connection reinforcement.

        } else if(nb1.hasNeighborAt(BOTTOM) || nb1.hasNeighborAt(LEFT) || nb1.hasNeighborAt(RIGHT) || nb1.hasNeighborAt(TOP)) {
            //penguin wrong
            currentBackgrounds[0] = 35;

            //audio: Positive reinforcement
        } else {
            //penguin idle
            currentBackgrounds[0] = 23;
        }

    } 
    //if ice1 was effected
    if(c0 == ice1Int || c1 == ice1Int){
        LOG("ICE1 EFFECTED\n");
        Neighborhood nb2(ice1);

        int bIce1 = int((nb2.neighborAt(BOTTOM)));
        int lIce1 = int((nb2.neighborAt(LEFT)));
        int rIce1 = int((nb2.neighborAt(RIGHT)));
        int tIce1 = int((nb2.neighborAt(TOP)));

        if(((lIce1 == pengInt) && (nb2.hasNeighborAt(BOTTOM) && !nb2.hasNeighborAt(RIGHT) && !nb2.hasNeighborAt(TOP))) ||
            ((rIce1 == ice2Int) && (nb2.hasNeighborAt(BOTTOM) && !nb2.hasNeighborAt(LEFT) && !nb2.hasNeighborAt(TOP)))) {
            //Right Ice1
            LOG("CORRECT ICE1 CONNECTION");
            // currentBackgrounds[1] = --;
        } else if(nb2.hasNeighborAt(BOTTOM) || nb2.hasNeighborAt(LEFT) || nb2.hasNeighborAt(RIGHT) || nb2.hasNeighborAt(TOP)){
            //WRONG Ice1
            currentBackgrounds[1] = 36;
        }else{
            //Idle Ice1
            currentBackgrounds[1] = 24;
        }

    }
    //If ice2 was effected
    if(c0 == ice2Int || c1 == ice2Int) {
        LOG("ICE2 EFFECTED\n");

        Neighborhood nb3(ice2);

        int bIce2 = int((nb3.neighborAt(BOTTOM)));
        int lIce2 = int((nb3.neighborAt(LEFT)));
        int rIce2 = int((nb3.neighborAt(RIGHT)));
        int tIce2 = int((nb3.neighborAt(TOP)));

        if(lIce2 == ice1Int && !nb3.hasNeighborAt(BOTTOM) && !nb3.hasNeighborAt(RIGHT) && !nb3.hasNeighborAt(TOP)){
            //Right Ice2
            LOG("CORRECT ICE2 CONNECTION");
            // currentBackgrounds[2] = --;
        } else if(nb3.hasNeighborAt(BOTTOM) || nb3.hasNeighborAt(LEFT) || nb3.hasNeighborAt(RIGHT) || nb3.hasNeighborAt(TOP)) {
            //ice2 wrong
            currentBackgrounds[2] = 37;

            //audio: Positive reinforcement
        } else {
            //ice2 idle
            currentBackgrounds[2] = 25;
        }


    }

    paintWrapper();

}

static void learningTask5(){

    //if monkey on giraffe & touching toucan
    if(false)
    {
        //Create Success Response, end game.
        LOG(":::::: You Passed Level 5 ::::::\nGREAT SUCCESS\n\n\n\n");
        
    } //Add further conditionals to reinforce negative feedback
    //else if (monkey touching giraffe not on top)
        //Then animate monkey and giraffe negatively
    //else if (giraffe touching toucan)
        //Then animate toucan and giraffe negatively
}

static void lt5SideCheck(unsigned cube0, unsigned cube1){

}