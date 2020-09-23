// Demo showing how to coordinate a global compass direction across a group of blinks.

// Each blink normally shows its own north face (face #0) in blue
// Pressing the button on a blink makes it the "center" and all the other
// connected blinks will sync to its "north" and show that in green.

// Each tile (starting with the center) sends out the compass angle on each face,
// so the center blink would send angle 0 on its face 0. When a tile recieves a
// compass angle from a parent, it uses it to compute which one of its own faces
// is pointing north, and then uses that to send a correct global angle to its children.
// For example, if a tile recieves an angle 0 on a face, then it knows the opsite face is
// pointing north (angle 0).

const byte NO_PARENT_FACE = FACE_COUNT ;   // Signals that we do not currently have a parent

byte parent_face = NO_PARENT_FACE;

Timer lockout_timer;    // This prevents us from forming packet loops by giving changes time to die out.
// Remember that timers init to expired state

byte the_north_face;    // Which one of our faces is pointing north? Only valid when parent_face != NO_PARENT_FACE

const byte IR_IDLE_VALUE = 7;   // A value we send when we do not know which way is north

const int LOCKOUT_TIMER_MS = 250;               // This should be long enough for very large loops, but short enough to be unnoticable

//------------------------------------------------------

bool amGod = false;
byte gravitySignal[6] = {IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE, IR_IDLE_VALUE};
byte godFaceOffset = 0;

enum blinkRoles {WALL, BUCKET, SPAWNER};
byte blinkRole = WALL;

bool bLongPress;
bool bChangeRole;


void setup() {
  // Function intentionally left blank
}



void loop() {

  setRole();

  if (buttonDoubleClicked()) {
    amGod = !amGod;
  }

  if (buttonSingleClicked() && !amGod) {
    godFaceOffset = (godFaceOffset + 1) % 6;
  }

  //send data
  FOREACH_FACE(f) {
    byte sendData = gravitySignal[f];
    setValueSentOnFace(sendData, f);
  }
}

void wallLoop() {
  if (bChangeRole) {
    blinkRole = BUCKET;
    bChangeRole = false;
  }

  setColor(OFF);

  amGod = false;


  FOREACH_FACE(f) {
    //    if (getGravitySignal(getLastValueReceivedOnFace(f)) == 6) {
    //      setColorOnFace(YELLOW, f);
    //    }

    if (isBucket(f)) { //do I have a neighbor and are they shouting IM_BUCKET?
      setColorOnFace(YELLOW, f);
      byte bucketNeighbor = (f + 2) % 6; //and what about their neighbor?
      if (isBucket(bucketNeighbor)) {
        setColor(WHITE);
        //        bFace = (f + 1) % 6;
        //        bottomFace = bFace;
        //        randomWallRole = 10;
        //        imSwitcher = true;
        amGod = true; //I get to decide what direction gravity is for the entire game! Yippee!
      }
    }
  }

  gravityLoop();

}

bool isBucket (byte face) {
  if (!isValueReceivedOnFaceExpired(face)) { //I have a neighbor
    if (getGravitySignal(getLastValueReceivedOnFace(face)) == 6) { //if the neighbor is a bucket, return true
      return true;
    } else if (getGravitySignal(getLastValueReceivedOnFace(face)) != 6) { //if the neighbor isn't a bucket, return false
      return false;
    }
  } else {
    return false; //or if I don't have a neighbor, that's also a false
  }
}

void bucketLoop() {
  if (bChangeRole) {
    blinkRole = SPAWNER;
    bChangeRole = false;
  }

  setColor(ORANGE);

  FOREACH_FACE(f) {
    gravitySignal[f] = 6; //6 means bucket, I don't make the rules
  }
}

void spawnerLoop() {
  if (bChangeRole) {
    blinkRole = WALL;
    bChangeRole = false;
  }

  setColor(GREEN);
}

void gravityLoop() {

  //  setColor( OFF );

  if (amGod) {

    // I am the center blink!

    FOREACH_FACE(f) {

      // Since by definition my face 0 is north, I can just send my face number

      //setValueSentOnFace( f , f );
      gravitySignal[f] = (f + 6 - godFaceOffset) % 6;

    }

    // This stuff is to reset everything clean for when the button is released....

    parent_face = NO_PARENT_FACE;

    lockout_timer.set(LOCKOUT_TIMER_MS);

    //setColor(dim(WHITE,128));

    setColorOnFace( WHITE , godFaceOffset );

    // That's all for the center blink!

  } else {

    // I am not the center blink!

    if (parent_face == NO_PARENT_FACE ) {   // If we have no parent, then look for one

      if (lockout_timer.isExpired()) {      // ...but only if we are not on lockout

        FOREACH_FACE(f) {

          if (!isValueReceivedOnFaceExpired(f)) {

            if (getGravitySignal(getLastValueReceivedOnFace(f)) < FACE_COUNT) {

              // Found a parent!

              parent_face = f;

              // Compute the oposite face of the one we are reading from

              byte our_oposite_face = (f + (FACE_COUNT / 2) ) % FACE_COUNT;

              // Grab the compass heading from the parent face we are facing
              // (this is also compass heading of our oposite face)

              byte parent_face_heading = getGravitySignal(getLastValueReceivedOnFace(f));

              // Ok, so now we know that `our_oposite_face` has a heading of `parent_face_heading`.

              // Compute which face is our north face from that

              the_north_face = ( (our_oposite_face + FACE_COUNT) - parent_face_heading) % FACE_COUNT;     // This +FACE_COUNT is to keep it positive

              // I guess we could break here, but breaks are ugly so instead we will keep looking
              // and use whatever the highest face with a parent that we find.
            }
          }
        }
      }

    } else {

      // Make sure our parent is still there and good

      if (isValueReceivedOnFaceExpired(parent_face) || ( getGravitySignal(getLastValueReceivedOnFace(parent_face )) == IR_IDLE_VALUE) ) {

        // We had a parent, but our parent is now gone

        parent_face = NO_PARENT_FACE;

        //setValueSentOnAllFaces( IR_IDLE_VALUE );  // Propigate the no-parentness to everyone resets viraly
        FOREACH_FACE(ff) {
          gravitySignal[ff] = IR_IDLE_VALUE;
        }

        lockout_timer.set(LOCKOUT_TIMER_MS);    // Wait this long before accepting a new parent to prevent a loop

      }

    }


    if (parent_face != NO_PARENT_FACE) {
      // We are synced to a remote center, so show the global north in green
      setColorOnFace( GREEN , the_north_face );
    } else {
      // Show our local north
      setColorOnFace( BLUE , godFaceOffset );
    }

    // Set our output values relative to our north

    FOREACH_FACE(f) {

      if (parent_face == NO_PARENT_FACE ||  f == parent_face ) {

        //setValueSentOnFace( IR_IDLE_VALUE , f );
        gravitySignal[f] = IR_IDLE_VALUE;
      } else {

        // Map directions onto our face indexes.
        // (It was surpsringly hard for my brain to wrap around this simple formula!)

        //setValueSentOnFace( ((f + FACE_COUNT) - the_north_face) % FACE_COUNT , f  );
        gravitySignal[f] = ((f + FACE_COUNT) - the_north_face) % FACE_COUNT;
      }

    }

  }
}




void setRole() {
  if (hasWoken()) {
    bLongPress = false;
  }

  if (buttonLongPressed()) {
    bLongPress = true;
  }

  if (buttonReleased()) {
    if (bLongPress) {
      //now change the role
      bChangeRole = true;
      bLongPress = false;
    }
  }

  switch (blinkRole) {
    case WALL:
      wallLoop();
      break;
    case BUCKET:
      bucketLoop();
      break;
    case SPAWNER:
      spawnerLoop();
      break;
  }

  if (bLongPress) {
    //transition color
    setColor(WHITE);
  }
}

byte getGravitySignal(byte data) { //all the gravity communication happens here
  return (data & 7);
}
