
//Laser Target App
//Kevin Darrah
//http://www.kdcircuits.com


//Libraries 
import processing.video.*;//you have to go and install this one
import static javax.swing.JOptionPane.*;//needed by the message box for selecting port
import processing.io.*;



Capture video;//the camera feed "video"

//variables that can be customized
int numberOfBlobs = 10;// number of blobs to have on the screen
int delayBetweenBlobs = 1000;// in milliseconds
int brightnessDiffThreshold = 60; //sensitivity of detection
int blobSizeThreshold = 100; //number of Pixels needed to have changed in order to run the blob detection function

int numPixels;//number pixels scanned each frame

PImage prescanImage;//this is the "old" frame saved
PImage changedImage;//modified frame from old vs new, used to detect the blob

// These are the coordiantes of the blobs
int[] brightSpotX = new int[numberOfBlobs];
int[] brightSpotY = new int[numberOfBlobs];

long startTime_shot = 0;//this is the timer variable for the delay between blobs
boolean firstFrame = true;//just so we can wipe out the changes from the first frame
long startShutdownTimer;
boolean shutdownTimerStarted = false;
void setup() {
  GPIO.pinMode(20, GPIO.INPUT_PULLUP);
  size(320, 240);//screen size

  //setup the camera
  cameraInit();

  prescanImage = createImage(320, 240, RGB);
  changedImage = createImage(320, 240, RGB);
  numPixels = 320*240;
}

void draw() {
  if (video.available()) {//only if a new frame is available
    checkBlob();
    drawScreen();
  }
  if (GPIO.digitalRead(20) == GPIO.LOW && shutdownTimerStarted==false) {
    startShutdownTimer=millis();
    shutdownTimerStarted=true;
    background(255, 0, 0);
  }
  if (GPIO.digitalRead(20) == GPIO.LOW && shutdownTimerStarted==true  && millis()-startShutdownTimer<=5000) {
    background(255, 0, 0);
  }
  else if (GPIO.digitalRead(20) == GPIO.LOW && shutdownTimerStarted==true && millis()-startShutdownTimer>5000) {
    background(0, 255, 0);
  }
  else if (GPIO.digitalRead(20) != GPIO.LOW && shutdownTimerStarted==true && millis()-startShutdownTimer>5000) {
    background(0, 255, 0);  
    println("Shutting Down");
    try{shutdown();
    }catch(IOException ex){}
    exit();

  } else {
    shutdownTimerStarted=false;
  }

}

public static void shutdown() throws IOException{
      try {
      Runtime.getRuntime().exec("sudo shutdown -h now");
    }
    catch(IOException ex) {
    }
  
}
