
#ifndef FLOPPY_CONTROL_H
#define FLOPPY_CONTROL_H

enum DriveSelect
{
	DRIVE_SELECT_NONE = 0,
	DRIVE_SELECT_A = 1,
	DRIVE_SELECT_B = 2,
};

enum Density
{
	DENSITY_DOUBLE,
	DENSITY_HIGH
};

void floppy_control_senseDrives();

void floppy_setMotor(enum DriveSelect drive, int val);
void floppy_selectDrive(enum DriveSelect sel);
void floppy_setHead(int head);
int floppy_stepToCylinder00();
void floppy_stepToCylinder(unsigned int wantedCyl);
void floppyControl_init();
void setupStepTimer(int waitTime);
int floppy_waitForIndex();
void floppy_setWriteGate(int val);
void floppy_measureRpm();
void floppy_selectDensity(enum Density val);
void floppy_selectFittingDrive();
void floppy_setMotorSelectedDrive(int val);

extern volatile unsigned int indexHappened;

#endif
