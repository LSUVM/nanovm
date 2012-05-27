//
//  NanoVMTool, Converter and Upload Tool for the NanoVM
//  Copyright (C) 2005-2006 by Till Harbaum <Till@Harbaum.org>
//                             Oliver Schulz <whisp@users.sf.net>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Parts of this tool are based on public domain code written by Kimberley
//  Burchett: http://www.kimbly.com/code/classfile/
//

import java.io.*;
import java.util.*;
import gnu.io.*;  // needs to be added to vm

public abstract class Uploader {

  private static Uploader uploader;

  protected Uploader() {
  }

  public abstract void doUpload(String device, int target, int speed, byte[] file, int length);


  public static void upload(String device, int target, int speed, byte[] file, int length){
    uploader.doUpload(device, target, speed, file, length);
  }


  public static void setUploader(String type){
    if (type.equals("nvc1")) {
      uploader = new UploaderNvmCom1();
    } else if (type.equals("nvc2")) {
      uploader = new UploaderNvmCom2();
    } else if (type.equals("ctbot")) {
      uploader = new UploaderCtBot();
    }
  }


  public static void main(String[] args) {
    byte[] buffer = null;

    if(args.length != 3) {
      System.out.println("Usage: Uploader device {bitrate|asuro} file");
      System.exit(-1);
    }
    
    // load file into memory
	// nvmcomm2 not enabled here yet.
    try {
      File inputFile = new File(args[2]);
      buffer = new byte[(int)inputFile.length()];      
      FileInputStream in = new FileInputStream(inputFile);
      in.read(buffer);
      in.close();
    } catch(IOException e) {
      System.out.println("Error loading file " + args[2]);
      System.exit(-1);      
    }

    setUploader("nvc1");

    if(args[1].equalsIgnoreCase("asuro"))
      upload(args[0], Config.TARGET_NVC1_ASURO, 2400, buffer, buffer.length);
    else
      upload(args[0], Config.TARGET_NVC1_UART, Integer.parseInt(args[1]), buffer, buffer.length);
  }
}
