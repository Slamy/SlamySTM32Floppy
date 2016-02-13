
#ifndef FLOPPY_CONTROL_H
#define FLOPPY_CONTROL_H

enum DriveSelect
{
	DRIVE_SELECT_NONE,
	DRIVE_SELECT_A,
	DRIVE_SELECT_B
};

void floppy_setMotor(int drive, int val);
void floppy_selectDrive(enum DriveSelect sel);
void floppy_setHead(int head);

//extern PT_THREAD(floppyControl_step_thread(struct pt *pt));



#endif
