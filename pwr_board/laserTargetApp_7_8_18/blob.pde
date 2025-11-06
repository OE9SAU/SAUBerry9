void checkBlob() {

  //if we see a big blob and there was enough time, since the last blob
  if (pixelsChanged()>blobSizeThreshold && millis()-startTime_shot >delayBetweenBlobs) {
    startTime_shot=millis();
    findBlob();//go find the blob!
  }

  //make a copy of the frame, so we can use it the next time around to check for changes
  prescanImage.copy(video, 0, 0, video.width, video.height, 0, 0, prescanImage.width, prescanImage.height);
  prescanImage.updatePixels();
}




int pixelsChanged() {
  video.read();
  video.loadPixels();//existing frame
  prescanImage.loadPixels();//last frame

  float pixelBrightness; 
  float oldPixelBrightness;
  int maxPixelsChanged = 0;//keep track of changed pixels, so dust won't trigger blob detection

  //sweep through entire frame comparing existing frame vs previous frame
  for (int y = 0; y< video.height; y++) {
    for (int x = 0; x< video.width; x++) {
      pixelBrightness = brightness(video.pixels[x+y*video.width]);
      oldPixelBrightness = brightness(prescanImage.pixels[x+y*video.width]);
      if (abs(pixelBrightness-oldPixelBrightness)>brightnessDiffThreshold) {// this is the threshold to detect a difference
        changedImage.pixels[x+y*video.width] = color(255, 0, 0);//set changed pixels red
        maxPixelsChanged++;
      } 
       else//if no change, then set to the frame, will look like raw video feed
        changedImage.pixels[x+y*video.width] = video.pixels[x+y*video.width];
    }
  }
  changedImage.updatePixels();//update the pixels, since we changed them

  return maxPixelsChanged;//return the number of pixels changed
}




//here's the magic function to find the blob centers
void findBlob() {
  int newXSpot= -10, newYSpot= -10;
  int furthestDistance = 0;
  int distanceTracker = 0;
  int xPixelForw=0;
  int xPixelBack=0;
  int yPixelUp=0;
  int yPixelDow=0;
  color spotColor = color(255, 0, 0);

  //scan through the changed image for the 'red' spots
  /* the idea here is to find, first the red pixel that is furthest from the non-red edges, that will give us a good
   *  starting point for the center, then second, we try to find that point that has equal distances away from non-red edges
   */
  for (int y = 0; y< video.height; y++) {
    for (int x = 0; x< video.width; x++) {

      //sweep through looking for red pixels, since this is where our blob lives
      if (color(changedImage.pixels[x+y*video.width])==spotColor) {
        //now we need to walk up and down/left right to find max pixel count away from non red
        for (int i=1; i<(video.width-x-1); i++) {//let's go forward on x
          if (color(changedImage.pixels[x+i+y*video.width])==spotColor)
            xPixelForw++;
          else
            i=video.width-x;
        }
        for (int i=1; i<x-1; i++) {//let's go backward on x
          if (color(changedImage.pixels[x-i+y*video.width])==spotColor)
            xPixelBack++;
          else
            i=x;
        }          
        for (int i=1; i<y-1; i++) {//let's go up on y
          if (color(changedImage.pixels[x+(y-i)*video.width])==spotColor)
            yPixelUp++;
          else
            i=y;
        }
        for (int i=1; i<video.height-y-1; i++) {//let's go down on y
          if (color(changedImage.pixels[x+(y+i)*video.width])==spotColor)
            yPixelDow++;
          else
            i=video.height-y;
        }
        distanceTracker = xPixelForw+xPixelBack+yPixelUp+yPixelDow;//adds up all of those distances from the non-red edges

        if (distanceTracker>furthestDistance) {//checks for biggest blob
          if (abs(xPixelForw-xPixelBack) < 10 && abs(yPixelUp-yPixelDow) < 10) {//checks for center here, with near-equal distances
            furthestDistance = distanceTracker;//log this as a contender for the big blob
            //set the x,y locations at the 0 position
            newXSpot=x;
            newYSpot=y;
          } else {//reset
            xPixelForw=0;
            xPixelBack=0;
            yPixelUp=0;
            yPixelDow=0;
          }
        } else {//reset
          xPixelForw=0;
          xPixelBack=0;
          yPixelUp=0;
          yPixelDow=0;
        }
      }
    }
  }

  //found a blob, but need to shift the 9 previous over in the array and roll off the last one
  for (int i=numberOfBlobs-1; i>0; i--) {
    brightSpotX[i] = brightSpotX[i-1];
    brightSpotY[i] = brightSpotY[i-1];
  }
  brightSpotX[0] = newXSpot;
  brightSpotY[0] = newYSpot;
}
