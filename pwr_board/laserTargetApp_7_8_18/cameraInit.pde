void cameraInit() {
  String[] cameras = Capture.list();//create array of available capture devices
  String CameraList="";//will be used for the input box

  if (cameras.length == 0) {
    println("There are no cameras available for capture.");
    exit();
  } else {
    println("Available cameras:");
    for (int i = 0; i < cameras.length; i++) {
     // println(cameras[i]);
      if (cameras[i].contains("size=320x240") && cameras[i].contains("fps=30")) {//only cameras we care about
        print(i);
        print(" ");
        CameraList = CameraList + i + "=" + cameras[i] + "\n";//giant array we post in the box
        println(cameras[i]);
        video = new Capture(this, cameras[i]);
        break;
      }
    }
//exit();
    //String CameraSelection="";//the integer we receive back from the box
    //CameraSelection = showInputDialog("Please Select Camera (0,1,2,3..):\n" + CameraList);
    //if (CameraSelection == null) exit();
    //if (CameraSelection.isEmpty()) exit();   
    //if (int(CameraSelection) >= cameras.length || int(CameraSelection)<0) exit();
    
  }

  video.start();
}
