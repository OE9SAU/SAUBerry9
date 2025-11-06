void drawScreen() {
  
  if (firstFrame) {
    firstFrame=false;
    for (int i=0; i<numberOfBlobs; i++) {//throw all of the coordinates for the blobs off-screen
      brightSpotX[i]=-10;
      brightSpotY[i]=-10;
    }
  }

  set(0, 0, changedImage);//set the screen to the changed image, so we can see the blobs
  //image(changedImage, 0,0,640,480);
  noStroke();
  fill(255, 0, 0, 127);//draw the old blobs first
  for (int i=1; i<numberOfBlobs; i++) {
    ellipse(brightSpotX[i], brightSpotY[i], 10, 10);
  }
  fill(0, 255, 0);
  ellipse(brightSpotX[0], brightSpotY[0], 10, 10);
}
