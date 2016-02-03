//--------------------------------------------------------------------------------------------------
// GoTo, commands to move the telescope to an location or to report the current location

// syncs the telescope/mount to the sky
boolean syncEqu(double RA, double Dec) {
  // hour angleTrackingMoveTo
  double HA=LST()*15.0-RA;
  while (HA>+180.0) HA-=360.0;
  while (HA<-180.0) HA+=360.0;

  // correct for polar misalignment only by clearing the index offsets
  IH=0; ID=0; IHS=0; IDS=0;

  double Axis1,Axis2;
#ifdef MOUNT_TYPE_ALTAZM
  EquToHor(latitude,HA,Dec,&Axis2,&Axis1); // convert from HA/Dec to Alt/Azm
  if (Axis1>180.0) Axis1-=360.0;
#else
  EquToCEqu(latitude,HA,Dec,&Axis1,&Axis2);
#endif

#ifdef SYNC_ANYWHERE_ON
  if ((pierSide==PierSideNone) || ((pierSide==PierSideWest) && (Axis1>0)) || ((pierSide==PierSideEast) && (Axis1<0))) {
    trackingState=TrackingSidereal;
    atHome=false;
    if (meridianFlip!=MeridianFlipNever) {
      // we're in the polar home position, so pick a side (of the pier)
      if (Axis1<0) {
        // west side of pier - we're in the eastern sky and the HA's are negative
        pierSide=PierSideWest;
        DecDir  =DecDirWInit;
      } else { 
        // east side of pier - we're in the western sky and the HA's are positive
        // this is the default in the polar-home position
        pierSide=PierSideEast;
        DecDir = DecDirEInit;
      }
    } else {
      // always on the "east" side of pier - we're in the western sky and the HA's are positive
      // this is the default in the polar-home position and also for MOUNT_TYPE_FORK and MOUNT_TYPE_ALTAZM.  MOUNT_TYPE_FORK_ALT ends up pierSideEast, but flips are allowed until aligned.
      pierSide=PierSideEast;
      DecDir = DecDirEInit;
    }
  }
#endif
  // compute index offsets IH/ID, if they're within reason 
  // actual posAxis1/posAxis2 are the coords of where this really is
  // IH/ID are the amount to add to the actual RA/Dec to arrive at the correct position
  // double's are really single's on the ATMega's, and we're a digit or two shy of what's required to
  // hold the steps in some cases but it's still getting down to the arc-sec level
  // HA goes from +180...0..-180
  //                 W   .   E
  // IH and ID values get subtracted to arrive at the correct location
  IH=Axis1-((double)(long)targetAxis1.part.m)/(double)StepsPerDegreeAxis1;
  IHS=(long)(IH*(double)StepsPerDegreeAxis1);
  ID=Axis2-((double)(long)targetAxis2.part.m)/(double)StepsPerDegreeAxis2;
  IDS=(long)(ID*(double)StepsPerDegreeAxis2);

#ifndef SYNC_ANYWHERE_ON
  if ((abs(ID)>30.0) || (abs(IH)>30.0)) { IH=0; ID=0; return false; }
#endif
  return true;
}

// this returns the telescopes HA and Dec (index corrected for Alt/Azm)
void getHADec(double *HA, double *Dec) {
  cli();
  long axis1=posAxis1;
  long axis2=posAxis2;
  sei();
  
#ifdef MOUNT_TYPE_ALTAZM
  // get the hour angle (or Azm)
  double z=(double)axis1/(double)StepsPerDegreeAxis1;
  // get the declination (or Alt)
  double a=(double)axis2/(double)StepsPerDegreeAxis2;

  // instrument to corrected horizon
  z+=IH;
  a+=ID;

  HorToEqu(latitude,a,z,HA,Dec); // convert from Alt/Azm to HA/Dec
#else
  // get the hour angle (or Azm)
  *HA=(double)axis1/(double)StepsPerDegreeAxis1;
  // get the declination (or Alt)
  *Dec=(double)axis2/(double)StepsPerDegreeAxis2;
#endif
}

// gets the telescopes current RA and Dec, set returnHA to true for Horizon Angle instead of RA
unsigned long lastGetEqu=0;
double lastGetEquLST=0;
double lastGetEquHA=0;
double lastGetEquDec=0;
boolean getEqu(double *RA, double *Dec, boolean returnHA, boolean fast) {
  unsigned long Tmin=250L; if (trackingState==TrackingMoveTo) Tmin=500L;
  
  // return last values if recent results apply
  if ((fast) && (millis()-lastGetEqu<Tmin)) {
    *Dec=lastGetEquDec;
    // return either the RA or the HA depending on returnHA
    if (!returnHA) {
      *RA=lastGetEquLST*15.0-lastGetEquHA;
      while (*RA>=360.0) *RA=*RA-360.0;
      while (*RA<0.0) *RA=*RA+360.0;
    } else *RA=lastGetEquHA;
    return true;
  }
  
  // get the HA and Dec (already index corrected on AltAzm)
  getHADec(&lastGetEquHA,Dec);
  
#ifndef MOUNT_TYPE_ALTAZM
  // correct for under the pole, polar misalignment, and index offsets
  CEquToEqu(latitude,lastGetEquHA,*Dec,&lastGetEquHA,Dec);
#endif

  lastGetEquLST=LST();
  lastGetEquDec=*Dec;
  lastGetEqu=millis();

  // return either the RA or the HA depending on returnHA
  if (!returnHA) {
    *RA=LST()*15.0-lastGetEquHA;
    while (*RA>=360.0) *RA=*RA-360.0;
    while (*RA<0.0) *RA=*RA+360.0;
  } else *RA=lastGetEquHA;
  
  return true;
}

// gets the telescopes current RA and Dec, set returnHA to true for Horizon Angle instead of RA
boolean getApproxEqu(double *RA, double *Dec, boolean returnHA) {
  double HA;
  
  // get the HA and Dec (already index corrected on AltAzm)
  getHADec(&HA,Dec);
  
#ifndef MOUNT_TYPE_ALTAZM
  // instrument to corrected equatorial
  HA+=IH;
  *Dec+=ID;
#endif
  
  // un-do, under the pole
  if (*Dec>90.0) { *Dec=(90.0-*Dec)+90; HA=HA-180.0; }
  if (*Dec<-90.0) { *Dec=(-90.0-*Dec)-90.0; HA=HA-180.0; }

  while (HA>180.0) HA=HA-360.0;
  while (HA<-180.0) HA=HA+360.0;
  if (*Dec>90.0) *Dec=+90.0;
  if (*Dec<-90.0) *Dec=-90.0;
  
  // return either the RA or the HA depending on returnHA
  if (!returnHA) {
    *RA=LST()*15.0-HA;
    while (*RA>=360.0) *RA=*RA-360.0;
    while (*RA<0.0) *RA=*RA+360.0;
  } else *RA=HA;
  return true;
}

// gets the telescopes current Alt and Azm
boolean getHor(double *Alt, double *Azm) {
  double h,d;
  getEqu(&h,&d,true,true);
  EquToHor(latitude,h,d,Alt,Azm);
  return true;
}

// moves the mount to a new Right Ascension and Declination (RA,Dec) in degrees
byte goToEqu(double RA, double Dec) {
  double a,z;

  // Convert RA into hour angle, get altitude
  double HA=LST()*15.0-RA;
  while (HA>=180.0) HA-=360.0;
  while (HA<-180.0) HA+=360.0;
  EquToHor(latitude,HA,Dec,&a,&z);

  // Check to see if this goto is valid
  if ((parkStatus!=NotParked) && (parkStatus!=Parking)) return 4;   // fail, Parked
  if (a<minAlt)                                         return 1;   // fail, below horizon
  if (a>maxAlt)                                         return 6;   // fail, outside limits
  if (Dec>MaxDec)                                       return 6;   // fail, outside limits
  if (Dec<MinDec)                                       return 6;   // fail, outside limits
  if (trackingState==TrackingMoveTo) { abortSlew=true;  return 5; } // fail, prior goto cancelled
  if (guideDirAxis1 || guideDirAxis2)                   return 7;   // fail, unspecified error

#ifdef MOUNT_TYPE_ALTAZM
  EquToHor(latitude,HA,Dec,&a,&z);
  if (z>180.0) z-=360.0;
  if (z<-180.0) z+=360.0;
  // corrected to instrument horizon
  z-=IH;
  a-=ID;
  long Axis1=z*(double)StepsPerDegreeAxis1;
  long Axis2=a*(double)StepsPerDegreeAxis2;
  long Axis1Alt=Axis1;
  long Axis2Alt=Axis2;
#else
  // correct for polar offset, refraction, coordinate systems, operation past pole, etc. as required
  double h,d;
  if (!EquToCEqu(latitude,HA,Dec,&h,&d)) return 2; // fail, coordinates invalid
  long Axis1=h*(double)StepsPerDegreeAxis1;
  long Axis2=d*(double)StepsPerDegreeAxis2;

  // as above... for the opposite pier side just incase we need to do a meridian flip
  byte p=pierSide; if (pierSide==PierSideEast) pierSide=PierSideWest; else if (pierSide==PierSideWest) pierSide=PierSideEast;
  if (!EquToCEqu(latitude,HA,Dec,&h,&d)) return 2; // fail, coordinates invalid

  long Axis1Alt=h*(double)StepsPerDegreeAxis1;
  long Axis2Alt=d*(double)StepsPerDegreeAxis2;
  pierSide=p;
#endif

  // goto function takes HA and Dec in steps
  // when in align mode, force pier side
  byte thisPierSide=PierSideBest;
  if ((alignMode==AlignOneStar1) || (alignMode==AlignTwoStar1) || (alignMode==AlignThreeStar1)) thisPierSide=PierSideWest;
  if ((alignMode==AlignTwoStar2) || (alignMode==AlignThreeStar2) || (alignMode==AlignThreeStar3)) thisPierSide=PierSideEast;
  return goTo(Axis1,Axis2,Axis1Alt,Axis2Alt,thisPierSide);
}

// moves the mount to a new Altitude and Azmiuth (Alt,Azm) in degrees
byte goToHor(double *Alt, double *Azm) {
  double HA,Dec;
  
  HorToEqu(latitude,*Alt,*Azm,&HA,&Dec);
  
  double RA=LST()*15.0-HA;
  while (RA>=360.0) RA=RA-360.0;
  while (RA<0.0) RA=RA+360.0;
  
  return goToEqu(RA,Dec);
}

// moves the mount to a new Hour Angle and Declination - both are in steps.  Alternate targets are used when a meridian flip occurs
byte goTo(long thisTargetAxis1, long thisTargetAxis2, long altTargetAxis1, long altTargetAxis2, byte gotoPierSide) {
  // HA goes from +90...0..-90
  //                W   .   E

  if (faultAxis1 || faultAxis2) return 7; // fail, unspecified error

  atHome=false;

  if (meridianFlip!=MeridianFlipNever) {
    // where the allowable hour angles are
    long eastOfPierMaxHA= UnderPoleLimit*15L*StepsPerDegreeAxis1;
    long eastOfPierMinHA=-(MinutesPastMeridianE*StepsPerDegreeAxis1/4L);
    long westOfPierMaxHA= (MinutesPastMeridianW*StepsPerDegreeAxis1/4L);
    long westOfPierMinHA=-UnderPoleLimit*15L*StepsPerDegreeAxis1;
  
    // override the defaults and force a flip if near the meridian and possible (for parking and align)
    if ((gotoPierSide!=PierSideBest) && (pierSide!=gotoPierSide)) {
      eastOfPierMinHA= (MinutesPastMeridianW*StepsPerDegreeAxis1/4L);
      westOfPierMaxHA=-(MinutesPastMeridianE*StepsPerDegreeAxis1/4L);
    }
    // if doing a meridian flip, use the opposite pier side coordinates
    if (pierSide==PierSideEast) {
      if ((thisTargetAxis1+IHS>eastOfPierMaxHA) || (thisTargetAxis1+IHS<eastOfPierMinHA)) {
        pierSide=PierSideFlipEW1;
        thisTargetAxis1 =altTargetAxis1;
        if (thisTargetAxis1+IHS>westOfPierMaxHA) return 6; // fail, outside limits
        thisTargetAxis2=altTargetAxis2;
      }
    } else
    if (pierSide==PierSideWest) {
      if ((thisTargetAxis1+IHS>westOfPierMaxHA) || (thisTargetAxis1+IHS<westOfPierMinHA)) {
        pierSide=PierSideFlipWE1; 
        thisTargetAxis1 =altTargetAxis1;
        if (thisTargetAxis1+IHS<eastOfPierMinHA) return 6; // fail, outside limits
        thisTargetAxis2=altTargetAxis2;
      }
    } else
    if (pierSide==PierSideNone) {
      // ID flips back and forth +/-, but that doesn't matter, if we're homed the ID is 0
      // we're in the polar home position, so pick a side (of the pier)
      if (thisTargetAxis1<0) {
        // west side of pier - we're in the eastern sky and the HA's are negative
        pierSide=PierSideWest;
        DecDir  =DecDirWInit;
        // default, if the polar-home position is +90 deg. HA, we want -90HA
        // for a fork mount the polar-home position is 0 deg. HA, so leave it alone
#if !defined(MOUNT_TYPE_FORK) && !defined(MOUNT_TYPE_FORK_ALT)
        cli(); posAxis1-=180L*StepsPerDegreeAxis1; sei();
        trueAxis1-=180L*StepsPerDegreeAxis1;
#endif
      } else {
        // east side of pier - we're in the western sky and the HA's are positive
        // this is the default in the polar-home position
        pierSide=PierSideEast;
        DecDir = DecDirEInit;
      }
    }
  } else {
    if (pierSide==PierSideNone) {
        // always on the "east" side of pier - we're in the western sky and the HA's are positive
        // this is the default in the polar-home position and also for MOUNT_TYPE_FORK and MOUNT_TYPE_ALTAZM.  MOUNT_TYPE_FORK_ALT ends up pierSideEast, but flips are allowed until aligned.
        pierSide=PierSideEast;
        DecDir = DecDirEInit;
    }
  }
  
  // final validation
 if (((thisTargetAxis1+IHS>StepsPerDegreeAxis1*180L) || (thisTargetAxis1+IHS<-StepsPerDegreeAxis1*180L)) ||
     ((thisTargetAxis2+IDS>StepsPerDegreeAxis2*180L) || (thisTargetAxis2+IDS<-StepsPerDegreeAxis2*180L))) return 7; // fail, unspecified error

  lastTrackingState=trackingState;

  cli();
  trackingState=TrackingMoveTo; 
  SetSiderealClockRate(siderealInterval);

  startAxis1=posAxis1;
  startAxis2=posAxis2;

  targetAxis1.part.m=thisTargetAxis1; targetAxis1.part.f=0;
  targetAxis2.part.m=thisTargetAxis2; targetAxis2.part.f=0;

  timerRateAxis1=SiderealRate;
  timerRateAxis2=SiderealRate;
  sei();
  
  DisablePec();
  
  return 0;
}
