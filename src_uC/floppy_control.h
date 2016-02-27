
#ifndef FLOPPY_CONTROL_H
#define FLOPPY_CONTROL_H

enum DriveSelect
{
	DRIVE_SELECT_NONE,
	DRIVE_SELECT_A,
	DRIVE_SELECT_B
};


enum Density
{
	DENSITY_DOUBLE,
	DENSITY_HIGH
};


void floppy_setMotor(int drive, int val);
void floppy_selectDrive(enum DriveSelect sel);
void floppy_setHead(int head);
void floppy_stepToCylinder00();
void floppy_stepToCylinder(unsigned int wantedCyl);
void floppyControl_init();
void setupStepTimer(int waitTime);
int floppy_waitForIndex();
void floppy_setWriteGate(int val);
void floppy_measureRpm();
void floppy_selectDensity(enum Density val);

extern volatile unsigned int indexHappened;

#endif
