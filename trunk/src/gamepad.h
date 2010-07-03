#ifndef _gamepad_h__
#define _gamepad_h__

typedef struct {
	int num_reports;

	int reportDescriptorSize;
	void *reportDescriptor; // must be in flash

	int deviceDescriptorSize; // if 0, use default
	void *deviceDescriptor; // must be in flash
	
	char (*init)(void);
	void (*update)(void);

	/* \brief Return true if events have occured on specified controller  */
	char (*changed)(char id);

	/**
	 * \param id controller id (starting at 1 to match report IDs)
	 * return The number of bytes written to buf
	 */
	char (*buildReport)(unsigned char *buf, char id);
} Gamepad;

#endif // _gamepad_h__


