
#ifndef FLOPPY_CONTROL_H
#define FLOPPY_CONTROL_H

#include "pt.h"
#include "pt-sem.h"

enum DriveSelect
{
	DRIVE_SELECT_NONE,
	DRIVE_SELECT_A,
	DRIVE_SELECT_B
};

void floppyControl_setMotor(int drive, int val);
void floppyControl_selectDrive(enum DriveSelect sel);

extern PT_THREAD(floppyControl_step_thread(struct pt *pt));



#endif
