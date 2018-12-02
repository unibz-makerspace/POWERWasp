/*
Modified by Dennis Patella WASPROJECT TEAM on 11/2014 
http://www.wasproject.it
 */
#include "Marlin.h"
#include "cardreader.h"
#include "ultralcd.h"
#include "stepper.h"
#include "temperature.h"
#include "language.h"

#ifdef SDSUPPORT


CardReader::CardReader()
{
   filesize = 0;
   sdpos = 0;
   sdprinting = false;
   cardOK = false;
   saving = false;
   logging = false;
   autostart_atmillis=0;
   workDirDepth = 0;
   file_subcall_ctr=0;
   memset(workDirParents, 0, sizeof(workDirParents));

   autostart_stilltocheck=true; //the SD start is delayed, because otherwise the serial cannot answer fast enough to make contact with the host software.
   lastnr=0;
  //power to SD reader
  #if SDPOWER > -1
    SET_OUTPUT(SDPOWER); 
    WRITE(SDPOWER,HIGH);
  #endif //SDPOWER
  
  autostart_atmillis=millis()+5000;
}

char *createFilename(char *buffer,const dir_t &p) //buffer>12characters
{
  char *pos=buffer;
  for (uint8_t i = 0; i < 11; i++) 
  {
    if (p.name[i] == ' ')continue;
    if (i == 8) 
    {
      *pos++='.';
    }
    *pos++=p.name[i];
  }
  *pos++=0;
  return buffer;
}


void  CardReader::lsDive(const char *prepend,SdFile parent)
{
  dir_t p;
 uint8_t cnt=0;
 
  while (parent.readDir(p, longFilename) > 0)
  {
    if( DIR_IS_SUBDIR(&p) && lsAction!=LS_Count && lsAction!=LS_GetFilename) // hence LS_SerialPrint
    {

      char path[13*2];
      char lfilename[13];
      createFilename(lfilename,p);
      
      path[0]=0;
      if(strlen(prepend)==0) //avoid leading / if already in prepend
      {
       strcat(path,"/");
      }
      strcat(path,prepend);
      strcat(path,lfilename);
      strcat(path,"/");
      
      //Serial.print(path);
      
      SdFile dir;
      if(!dir.open(parent,lfilename, O_READ))
      {
        if(lsAction==LS_SerialPrint)
        {
          SERIAL_ECHO_START;
          SERIAL_ECHOLN(MSG_SD_CANT_OPEN_SUBDIR);
          SERIAL_ECHOLN(lfilename);
        }
      }
      lsDive(path,dir);
      //close done automatically by destructor of SdFile

      
    }
    else
    {
      if (p.name[0] == DIR_NAME_FREE) break;
      if (p.name[0] == DIR_NAME_DELETED || p.name[0] == '.'|| p.name[0] == '_') continue;
      if (longFilename[0] != '\0' &&
          (longFilename[0] == '.' || longFilename[0] == '_')) continue;
      if ( p.name[0] == '.')
      {
        if ( p.name[1] != '.')
        continue;
      }
      
      if (!DIR_IS_FILE_OR_SUBDIR(&p)) continue;
      filenameIsDir=DIR_IS_SUBDIR(&p);
      
      
      if(!filenameIsDir)
      {
        if(p.name[8]!='G') continue;
        if(p.name[9]=='~') continue;
      }
      //if(cnt++!=nr) continue;
      createFilename(filename,p);
      if(lsAction==LS_SerialPrint)
      {
        SERIAL_PROTOCOL(prepend);
        SERIAL_PROTOCOLLN(filename);
      }
      else if(lsAction==LS_Count)
      {
        nrFiles++;
      } 
      else if(lsAction==LS_GetFilename)
      {
        if(cnt==nrFiles)
          return;
        cnt++;
        
      }
    }
  }
}

void CardReader::ls() 
{
  lsAction=LS_SerialPrint;
  if(lsAction==LS_Count)
  nrFiles=0;

  root.rewind();
  lsDive("",root);
}


void CardReader::initsd()
{
  cardOK = false;
  if(root.isOpen())
    root.close();
#ifdef SDSLOW
  if (!card.init(SPI_HALF_SPEED,SDSS))
#else
  if (!card.init(SPI_FULL_SPEED,SDSS))
#endif
  {
    //if (!card.init(SPI_HALF_SPEED,SDSS))
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM(MSG_SD_INIT_FAIL);
  }
  else if (!volume.init(&card))
  {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_VOL_INIT_FAIL);
  }
  else if (!root.openRoot(&volume)) 
  {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_OPENROOT_FAIL);
  }
  else 
  {
    cardOK = true;
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM(MSG_SD_CARD_OK);
  }
  workDir=root;
  curDir=&root;
  /*
  if(!workDir.openRoot(&volume))
  {
    SERIAL_ECHOLNPGM(MSG_SD_WORKDIR_FAIL);
  }
  */
  
}

void CardReader::setroot()
{
  /*if(!workDir.openRoot(&volume))
  {
    SERIAL_ECHOLNPGM(MSG_SD_WORKDIR_FAIL);
  }*/
  workDir=root;
  
  curDir=&workDir;
}
void CardReader::release()
{
  sdprinting = false;
  cardOK = false;
}

void CardReader::startFileprint()
{
  if(cardOK)
  {
    sdprinting = true;
  }
}

void CardReader::pauseSDPrint()
{
  if(sdprinting)
  {
    sdprinting = false;
  }
}


void CardReader::openLogFile(char* name)
{
  logging = true;
  openFile(name, false);
}

void CardReader::getAbsFilename(char *t)
{
  uint8_t cnt=0;
  *t='/';t++;cnt++;
  for(uint8_t i=0;i<workDirDepth;i++)
  {
    workDirParents[i].getFilename(t); //SDBaseFile.getfilename!
    while(*t!=0 && cnt< MAXPATHNAMELENGTH) 
    {t++;cnt++;}  //crawl counter forward.
  }
  if(cnt<MAXPATHNAMELENGTH-13)
    file.getFilename(t);
  else
    t[0]=0;
}

void CardReader::openFile(char* name,bool read, bool replace_current/*=true*/)
{
  if(!cardOK)
    return;
  if (!file.exists("resurr.g")) {
    	file.createContiguous(&workDir,"resurr.g",1);
    	file.close();
    	
   }
  if(file.isOpen())  //replacing current file by new file, or subfile call
  {
    if(!replace_current)
    {
     if((int)file_subcall_ctr>(int)SD_PROCEDURE_DEPTH-1)
     {
       SERIAL_ERROR_START;
       SERIAL_ERRORPGM("trying to call sub-gcode files with too many levels. MAX level is:");
       SERIAL_ERRORLN(SD_PROCEDURE_DEPTH);
       kill();
       return;
     }
     
     SERIAL_ECHO_START;
     SERIAL_ECHOPGM("SUBROUTINE CALL target:\"");
     SERIAL_ECHO(name);
     SERIAL_ECHOPGM("\" parent:\"");
     
     //store current filename and position
     getAbsFilename(filenames[file_subcall_ctr]);
     
     SERIAL_ECHO(filenames[file_subcall_ctr]);
     SERIAL_ECHOPGM("\" pos");
     SERIAL_ECHOLN(sdpos);
     filespos[file_subcall_ctr]=sdpos;
     file_subcall_ctr++;
    }
    else
    {
     SERIAL_ECHO_START;
     SERIAL_ECHOPGM("Now doing file: ");
     SERIAL_ECHOLN(name);
     //memset(filename, 0, sizeof(filename));   //DEN AGGIUNTE
     for (int c=0; c<=sizeof(filename); c++)
  		const_cast<char&>(filename[c]) = '\0';
     strncpy(filename, name, strlen(name));
     SERIAL_ECHO("--- sizeof filename: ");
     SERIAL_ECHOLN(sizeof(filename));
     SERIAL_ECHO("--- strlen filename: ");
     SERIAL_ECHOLN(strlen(filename));
     SERIAL_ECHO("--- sizeof name: ");
     SERIAL_ECHOLN(sizeof(name));
     SERIAL_ECHO("--- strlen name: ");
     SERIAL_ECHOLN(strlen(name));
     
     
     
     }
    file.close();
  }
  else //opening fresh file
  {
    file_subcall_ctr=0; //resetting procedure depth in case user cancels print while in procedure
    SERIAL_ECHO_START;
    SERIAL_ECHOPGM("Now fresh file: ");
    SERIAL_ECHOLN(name);
    
     for (int c=0; c<=sizeof(filename); c++)
     const_cast<char&>(filename[c]) = '\0';
     strncpy(filename, name, strlen(name));
     SERIAL_ECHO("--- sizeof filename: ");
     SERIAL_ECHOLN(sizeof(filename));
     SERIAL_ECHO("--- strlen filename: ");
     SERIAL_ECHOLN(strlen(filename));
     SERIAL_ECHO("--- sizeof name: ");
     SERIAL_ECHOLN(sizeof(name));
     SERIAL_ECHO("--- strlen name: ");
     SERIAL_ECHOLN(strlen(name));
     
     
	
	
    }
  
  sdprinting = false;
  
 
  SdFile myDir;
  curDir=&root;
  char *fname=name;
  
  char *dirname_start,*dirname_end;
  if(name[0]=='/')
  {
    dirname_start=strchr(name,'/')+1;
    while(dirname_start>0)
    {
      dirname_end=strchr(dirname_start,'/');
      //SERIAL_ECHO("start:");SERIAL_ECHOLN((int)(dirname_start-name));
      //SERIAL_ECHO("end  :");SERIAL_ECHOLN((int)(dirname_end-name));
      if(dirname_end>0 && dirname_end>dirname_start)
      {
        char subdirname[13];
        strncpy(subdirname, dirname_start, dirname_end-dirname_start);
        subdirname[dirname_end-dirname_start]=0;
        SERIAL_ECHOLN(subdirname);
        if(!myDir.open(curDir,subdirname,O_READ))
        {
          SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
          SERIAL_PROTOCOL(subdirname);
          SERIAL_PROTOCOLLNPGM(".");
          return;
        }
        else
        {
          //SERIAL_ECHOLN("dive ok");
        }
          
        curDir=&myDir; 
        dirname_start=dirname_end+1;
      }
      else // the reminder after all /fsa/fdsa/ is the filename
      {
        fname=dirname_start;
        //SERIAL_ECHOLN("remaider");
        //SERIAL_ECHOLN(fname);
        break;
      }
      
    }
  }
  else //relative path
  {
    curDir=&workDir;
  }
  if(read)
  {
    if (file.open(curDir, fname, O_READ)) 
    {
      filesize = file.fileSize();
      SERIAL_PROTOCOLPGM(MSG_SD_FILE_OPENED);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLPGM(MSG_SD_SIZE);
      SERIAL_PROTOCOLLN(filesize);
      sdpos = 0;
      
      SERIAL_PROTOCOLLNPGM(MSG_SD_FILE_SELECTED);
      lcd_setstatus(fname);
    }
    else
    {
      SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLLNPGM(".");
    }
  }
  else 
  { //write
    if (!file.open(curDir, fname, O_CREAT | O_APPEND | O_WRITE | O_TRUNC))
    {
      SERIAL_PROTOCOLPGM(MSG_SD_OPEN_FILE_FAIL);
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLLNPGM(".");
    }
    else
    {
      saving = true;
      SERIAL_PROTOCOLPGM(MSG_SD_WRITE_TO_FILE);
      SERIAL_PROTOCOLLN(name);
      lcd_setstatus(fname);
    }
  }
  
}

void CardReader::removeFile(char* name)
{
  if(!cardOK)
    return;
  file.close();
  sdprinting = false;
  
  
  SdFile myDir;
  curDir=&root;
  char *fname=name;
  
  char *dirname_start,*dirname_end;
  if(name[0]=='/')
  {
    dirname_start=strchr(name,'/')+1;
    while(dirname_start>0)
    {
      dirname_end=strchr(dirname_start,'/');
      //SERIAL_ECHO("start:");SERIAL_ECHOLN((int)(dirname_start-name));
      //SERIAL_ECHO("end  :");SERIAL_ECHOLN((int)(dirname_end-name));
      if(dirname_end>0 && dirname_end>dirname_start)
      {
        char subdirname[13];
        strncpy(subdirname, dirname_start, dirname_end-dirname_start);
        subdirname[dirname_end-dirname_start]=0;
        SERIAL_ECHOLN(subdirname);
        if(!myDir.open(curDir,subdirname,O_READ))
        {
          SERIAL_PROTOCOLPGM("open failed, File: ");
          SERIAL_PROTOCOL(subdirname);
          SERIAL_PROTOCOLLNPGM(".");
          return;
        }
        else
        {
          //SERIAL_ECHOLN("dive ok");
        }
          
        curDir=&myDir; 
        dirname_start=dirname_end+1;
      }
      else // the reminder after all /fsa/fdsa/ is the filename
      {
        fname=dirname_start;
        //SERIAL_ECHOLN("remaider");
        //SERIAL_ECHOLN(fname);
        break;
      }
      
    }
  }
  else //relative path
  {
    curDir=&workDir;
  }
    if (file.remove(curDir, fname)) 
    {
      SERIAL_PROTOCOLPGM("File deleted:");
      SERIAL_PROTOCOL(fname);
      sdpos = 0;
    }
    else
    {
      SERIAL_PROTOCOLPGM("Deletion failed, File: ");
      SERIAL_PROTOCOL(fname);
      SERIAL_PROTOCOLLNPGM(".");
    }
  
}

void CardReader::getStatus()
{
  if(cardOK){
    SERIAL_PROTOCOLPGM(MSG_SD_PRINTING_BYTE);
    SERIAL_PROTOCOL(sdpos);
    SERIAL_PROTOCOLPGM("/");
    SERIAL_PROTOCOLLN(filesize);
  }
  else{
    SERIAL_PROTOCOLLNPGM(MSG_SD_NOT_PRINTING);
  }
}
void CardReader::write_command(char *buf)
{
  char* begin = buf;
  char* npos = 0;
  char* end = buf + strlen(buf) - 1;

  file.writeError = false;
  if((npos = strchr(buf, 'N')) != NULL)
  {
    begin = strchr(npos, ' ') + 1;
    end = strchr(npos, '*') - 1;
  }
  end[1] = '\r';
  end[2] = '\n';
  end[3] = '\0';
  file.write(begin);
  if (file.writeError)
  {
    SERIAL_ERROR_START;
    SERIAL_ERRORLNPGM(MSG_SD_ERR_WRITE_TO_FILE);
  }
}


void CardReader::checkautostart(bool force)
{
  if(!force)
  {
    if(!autostart_stilltocheck)
      return;
    if(autostart_atmillis<millis())
      return;
  }
  autostart_stilltocheck=false;
  if(!cardOK)
  {
    initsd();
    if(!cardOK) //fail
      return;
  }
  
  char autoname[30];
  sprintf_P(autoname, PSTR("auto%i.g"), lastnr);
  for(int8_t i=0;i<(int8_t)strlen(autoname);i++)
    autoname[i]=tolower(autoname[i]);
  dir_t p;

  root.rewind();
  
  bool found=false;
  while (root.readDir(p, NULL) > 0) 
  {
    for(int8_t i=0;i<(int8_t)strlen((char*)p.name);i++)
    p.name[i]=tolower(p.name[i]);
    //Serial.print((char*)p.name);
    //Serial.print(" ");
    //Serial.println(autoname);
    if(p.name[9]!='~') //skip safety copies
    if(strncmp((char*)p.name,autoname,5)==0)
    {
      char cmd[30];

      sprintf_P(cmd, PSTR("M23 %s"), autoname);
      enquecommand(cmd);
      enquecommand_P(PSTR("M24"));
      found=true;
    }
  }
  if(!found)
    lastnr=-1;
  else
    lastnr++;
}

void CardReader::closefile(bool saving_data)
{	

  file.sync();
  file.close();
  saving = false; 
  logging = false;
  
  
  //******     SALVATAGGIO FILE DENNIS 10/10/2014      ******//
  
  
  if(saving_data)
  {
  	 char bufCommand[50];
	 char bufX[11];
	 char bufY[11];
	 char bufZ[11];
	 char bufE[11];
	 char bufCoord[50];
	 char bufCoord1[50];
	 char bufCoord2[50];
	 char bufSdpos[11];
	 char nameSav[15];
	 snprintf(bufSdpos, sizeof bufSdpos, "%lu", (unsigned long)sdpos); 

	 
	 strcpy(nameSav, "resurr.g");
    if (!fileSav.exists(nameSav)) {
    	fileSav.createContiguous(&workDir,nameSav,1);
    	fileSav.close();
    	}
    
    	 fileSav.open(&workDir,nameSav,O_WRITE);
    	 fileSav.truncate(0);
	 
	 dtostrf(current_position[X_AXIS],1,3,bufX);
	 dtostrf(current_position[Y_AXIS],1,3,bufY);
	 dtostrf(current_position[Z_AXIS],1,3,bufZ);
	 dtostrf(current_position[E_AXIS],1,3,bufE);
#ifdef DELTA	 
	 strcpy(bufCoord1,"G1 Z");
	 strcat(bufCoord1,bufZ);
	 strcat(bufCoord1," F10000");
#else
	 strcpy(bufCoord1,"G92 Z");
	 strcat(bufCoord1,bufZ);
#endif
	 strcpy(bufCoord,"G1 X");
	 strcat(bufCoord,bufX);
	 strcat(bufCoord," Y");
	 strcat(bufCoord,bufY);
	 strcat(bufCoord," Z");
	 strcat(bufCoord,bufZ);
	 strcat(bufCoord," F10000");
	 strcpy(bufCoord2,"G92 E");
	 strcat(bufCoord2,bufE);
	 for(int8_t i=0;i<(int8_t)strlen(filename);i++)
    	 filename[i]=tolower(filename[i]);
    
	 strcpy(bufCommand,"M32 S");
	 strcat(bufCommand,bufSdpos);
	 strcat(bufCommand," !");
	 strcat(bufCommand,filename);
#ifdef DELTA
	 fileSav.write("G28\n");
#else
	fileSav.write(bufCoord1);
	fileSav.write("\n");
	fileSav.write("G28 X0 Y0\n");
#endif
	if (!clayMode) { 
		if (savingTempBED>0) {
	 		char Btemp[15];	
	 		sprintf(Btemp,"M190 S%u\n",savingTempBED);
	 		fileSav.write(Btemp);
		}
		if (savingTempEX1>0) {
	 		char E1temp[15];	
	 		sprintf(E1temp,"M109 S%u\n",savingTempEX1);
	 		fileSav.write(E1temp);
		} 
	} else fileSav.write("M302\n");	 
#ifdef DELTA
	 	fileSav.write(bufCoord1);
       		fileSav.write("\n");
#endif
       		fileSav.write(bufCoord);
       		fileSav.write("\n");
   	if (fanSpeed>DEFAULTFAN) {
	 	char fanSp[15];	
	 	sprintf(fanSp,"M106 S%u\n",fanSpeed);
	 	fileSav.write(fanSp);
	}
       		fileSav.write(bufCoord2);
       		fileSav.write("\n");
    	 	fileSav.write(bufCommand);
       		fileSav.write("\n");
	 
	 
    	 	fileSav.sync();
    	 	fileSav.close();
    	 //delay(300);
#ifdef DELTA
    	enquecommand_P(PSTR("G28"));
#else
	enquecommand_P(PSTR("G28 X0 Y0"));
#endif
    	 if (!clayMode) { 
    	 	setTargetHotend0(0);
       		setTargetHotend1(0);
         	setTargetHotend2(0);
         	setTargetBed(0);
        }
        fanSpeed=DEFAULTFAN;
         
    
    
  }

  
}

void CardReader::getfilename(const uint8_t nr)
{
  curDir=&workDir;
  lsAction=LS_GetFilename;
  nrFiles=nr;
  curDir->rewind();
  lsDive("",*curDir);
  
}

uint16_t CardReader::getnrfilenames()
{
  curDir=&workDir;
  lsAction=LS_Count;
  nrFiles=0;
  curDir->rewind();
  lsDive("",*curDir);
  //SERIAL_ECHOLN(nrFiles);
  return nrFiles;
}

void CardReader::chdir(const char * relpath)
{
  SdFile newfile;
  SdFile *parent=&root;
  
  if(workDir.isOpen())
    parent=&workDir;
  
  if(!newfile.open(*parent,relpath, O_READ))
  {
   SERIAL_ECHO_START;
   SERIAL_ECHOPGM(MSG_SD_CANT_ENTER_SUBDIR);
   SERIAL_ECHOLN(relpath);
  }
  else
  {
    if (workDirDepth < MAX_DIR_DEPTH) {
      for (int d = ++workDirDepth; d--;)
        workDirParents[d+1] = workDirParents[d];
      workDirParents[0]=*parent;
    }
    workDir=newfile;
  }
}

void CardReader::updir()
{
  if(workDirDepth > 0)
  {
    --workDirDepth;
    workDir = workDirParents[0];
    int d;
    for (int d = 0; d < workDirDepth; d++)
      workDirParents[d] = workDirParents[d+1];
  }
}


void CardReader::printingHasFinished()
{
    st_synchronize();
    if(file_subcall_ctr>0) //heading up to a parent file that called current as a procedure.
    {
      file.close();
      file_subcall_ctr--;
      openFile(filenames[file_subcall_ctr],true,true);
      setIndex(filespos[file_subcall_ctr]);
      startFileprint();
    }
    else
    {
      quickStop();
      file.close();
      sdprinting = false;
      if(SD_FINISHED_STEPPERRELEASE)
      {
          //finishAndDisableSteppers();
          enquecommand_P(PSTR(SD_FINISHED_RELEASECOMMAND));
      }
      autotempShutdown();
    }
}
#endif //SDSUPPORT


