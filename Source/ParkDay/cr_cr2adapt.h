/*!
 *******************************************************************************
                                                                                
                   N1P/N3P Sensor CR2 Adapter
                                                                                
  	 @section copyright_notice COPYRIGHT NOTICE
  	     Copyright (c) 2010 by Creatz Inc. 
  		 All Rights Reserved. \n
  		 Do not duplicate without prior written consent of Creatz Inc.

 *******************************************************************************
  	 @section file_information FILE CREATION INFORMATION
	 @file   cr_cr2adapt.h
	 @brief  N1P/N3P Sensor Adapter 
	 @author YongHo Suk  (yhsuk@mycreatz.com)
	 @date   2010/11/04 First Created

	 @section checkin_information LATEST CHECK-IN INFORMATION
	$Rev$ 
	$Author$
	$Date$
 *******************************************************************************/

#if !defined(_CR_CR2ADAPT_)
#define		 _CR_CR2ADAPT_

/*----------------------------------------------------------------------------
 	Description	: defines referenced header files  
-----------------------------------------------------------------------------*/
#include <windows.h>

#if defined (__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------------
 *	Description	: defines Macros and definitions
 -----------------------------------------------------------------------------*/

#if defined(CR_CR2ADAPT_DLL_IMPLEMENT)
#define CR_CR2ADAPT_EXPORT extern "C" __declspec(dllexport)
#else
#define CR_CR2ADAPT_EXPORT extern "C" __declspec(dllimport)
#endif

//---------- Result Code (CR2_result_t)
#define		CR2_OK						0x00000000

#define		CR2_ERR_GENERAL				0x80000000
#define		CR2_ERR_BADHANDLE			0x80000001
#define		CR2_ERR_UNSUPPORTED_CMD		0x80000010
#define		CR2_ERR_BADPARAM			0x80000011
#define		CR2_ERR_CAM_RESERVE_FAIL	0x80000020
#define		CR2_ERR_CAM_NOT_OWNER		0x80000021
#define		CR2_ERR_CAM_CONFIG_FAIL		0x80000022

#define		CR2_IS_RESULT_ERROR(x)	(CR2_ERR_GENERAL & (x))


//---------- Sensor Status
#define		CR2STATUS_NULL				0x00000000
#define		CR2STATUS_READY				0x00000001
#define		CR2STATUS_GOODSHOT			0x00000010
#define		CR2STATUS_TRIALSHOT			0x00000011
#define		CR2STATUS_DISCONNECT		0x00000100
#define		CR2STATUS_BIGSHADOW			0x00000101
#define		CR2STATUS_NOBALL			0x00000102
#define		CR2STATUS_LOWACTIVE			0x00000103
#define		CR2STATUS_REBOOT			0x00000104

//---------- Club Code
#define		CR2CLUB_WOOD1				1
#define		CR2CLUB_DRIVER				CR2CLUB_WOOD1
#define		CR2CLUB_WOOD2				2
#define		CR2CLUB_WOOD3				3
#define		CR2CLUB_WOOD4				4
#define		CR2CLUB_WOOD5				5
#define		CR2CLUB_WOOD6				6
#define		CR2CLUB_WOOD7				7
#define		CR2CLUB_WOOD8				8
#define		CR2CLUB_WOOD9				9

#define		CR2CLUB_IRON1				11
#define		CR2CLUB_IRON2				12
#define		CR2CLUB_IRON3				13
#define		CR2CLUB_IRON4				14
#define		CR2CLUB_IRON5				15
#define		CR2CLUB_IRON6				16
#define		CR2CLUB_IRON7				17
#define		CR2CLUB_IRON8				18
#define		CR2CLUB_IRON9				19
#define		CR2CLUB_IRONP				20
#define		CR2CLUB_IRONG				21
#define		CR2CLUB_IRONS				22
#define		CR2CLUB_IRONL				23

#define		CR2CLUB_PUTTER				30
	
#define		CR2CLUB_LADY_ADD			1000
#define		CR2CLUB_LADY(x)				((x) + CR2CLUB_LADY_ADD)

#define		CR2CLUBCALC_DEFAULT				0
#define		CR2CLUBCALC_NOCALC				1
#define		CR2CLUBCALC_CLUB_NOMARK			2
#define		CR2CLUBCALC_CLUB_MARK1			3
#define		CR2CLUBCALC_CLUB_MARK2			4

//---------- Command Code
#define		CR2CMD_NULL					0x00000000
#define		CR2CMD_DLLVERSION			0x00000001
#define		CR2CMD_SENSORCONFIG			0x00000002
#define		CR2CMD_CAMSENSORCONFIG		0x00000003
#define		CR2CMD_OPERATION_START		0x00000010				// p0: callbackfuncion.   p1; user param.
#define		CR2CMD_OPERATION_STOP		0x00000011
#define		CR2CMD_OPERATION_RESTART	0x00000012					
#define		CR2CMD_OPERATION_ACTIVATE	0x00000013				// p0, 0: No operation,   1: Normal operation
#define		CR2CMD_OPERATION_START1		0x00000014				// p0: callbackfuncion1.   p1: callbackfunction1_id p1; user param.
#define		CR2CMD_USETEE				0x00000020
#define		CR2CMD_USECLUB				0x00000021
#define		CR2CMD_WIND					0x00000022
#define		CR2CMD_SETTEESTATE			0x00000023
#define		CR2CMD_SETRIGHTLEFT			0x00000024				// p0, 0: Right-handed sensor, 1: Left-handed sensor


#define		CR2CMD_SENSORSTATUS			0x00000030
#define		CR2CMD_SENSORSTATUS2		0x00000031
#define		CR2CMD_AREAALLOW			0x00000032
#define		CR2CMD_BALLPOSITION			0x00000033
#define		CR2CMD_SHOTRESULTEX			0x00000034				// 	p0, 0: Don't clear result, 1: Clear result
																//  p1: CR2_shotdata_ballEx_t
#define		CR2CMD_BALLPOSITION3D		0x00000035				//  20200730
#define		CR2CMD_SHOTRESULTFULL		0x00000036				// 	p0: CR2_shotdata_full_t
#define		CR2CMD_CALC_TRAJECTORY		0x00000040
#define		CR2CMD_SET_AIRPRESSURE_PA	0x00000041				// p0: air pressure in Pa 
#define		CR2CMD_SET_ALTITUDE			0x00000042				// p0: altitutde in m 
#define		CR2CMD_SET_CLUBMODE			0x00000043				// p0: club mode.   0: Default. 1: NO Club CALC. 2: NO Club Marking, 3: Club Marking #1, 4: Club Marking #2

#define		CR2CMD_SET_CALLBACK			0x00000048				// p0: new callback func. p1: callback function id    20200513

#define		CR2CMD_CAM_RESERVE_CH		0x00000050
#define		CR2CMD_CAM_RELEASE_CH		0x00000051
#define		CR2CMD_CAM_CONFIG			0x00000052
#define		CR2CMD_CAM_OPERATION_START	0x00000053
#define		CR2CMD_CAM_OPERATION_STOP	0x00000054

//----
#define		CR2CMD_SWINGCAM_OPEN		0x00000060
#define		CR2CMD_SWINGCAM_CLOSE		0x00000061
#define		CR2CMD_SWINGCAM_START		0x00000062
#define		CR2CMD_SWINGCAM_STOP		0x00000063
#define		CR2CMD_SWINGCAM_READYREPLAY	0x00000064
#define		CR2CMD_SWINGCAM_MANUALREPLAY	0x00000065
#define		CR2CMD_SWINGCAM_AUTOREPLAY	0x00000066
#define		CR2CMD_SWINGCAM_LOADPLAY	0x00000067

//----		2012/0927
#define		CR2CMD_AUTOTEE_OPEN			0x00000070
#define		CR2CMD_AUTOTEE_CLOSE		0x00000071
#define		CR2CMD_AUTOTEE_SETHEIGHT	0x00000072
#define		CR2CMD_AUTOTEE_GETDATA		0x00000073
#define		CR2CMD_AUTOTEE_GETHEIGHT	0x00000074
#define		CR2CMD_AUTOTEE_GETEVENT		0x00000075
#define		CR2CMD_AUTOTEE_SENDCMD		0x00000076

//----		2012/1009
#define		CR2CMD_SWINGPLATE_OPEN		0x00000080
#define		CR2CMD_SWINGPLATE_CLOSE		0x00000081
#define		CR2CMD_SWINGPLATE_SETDATA	0x00000082
#define		CR2CMD_SWINGPLATE_GETSTATE	0x00000083

//----		2016/0304
#define		CR2CMD_SHOTDATAEX			0x00000090			// Get Extended Shot data
#define		CR2CMD_RIGHTLEFTCONNECTION	0x00000091			// p0: right sensor is connected,  p1: left sensor is connected
#define		CR2CMD_IRONAREAALLOW		0x00000092			// p0: Fairway is allowed. p1: Rough is allowed. p2: Bunker is allowed

//----		2018/0531
#define		CR2CMD_RESETSENSOR			0x00000098			// RESET and REBOOT SENSOR board
#define		CR2CMD_CLEARLAN				0x00000099			// RESET and CLEAR LAN

//----		2019/0508
#define		CR2CMD_GETSWSPEC			0x000000A0			// p0: U08 swspec[64+4]
#define		CR2CMD_GETBARCODE			0x000000A1			// p0: U08 barcode[64]
#define		CR2CMD_SETCURRENTDIRSTR		0x000000A2			// p0: TCHAR currentdir[1024]

//----		2020/0331
#define		CR2CMD_RGB0_RGB		        0x000000B0			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
															//				 p1: (((R << 16) & 0x00FF0000) | ((G << 8) & 0x0000FF00) | (B & 0x000000FF))
#define		CR2CMD_RGB1_RGB		        0x000000B1			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
#define		CR2CMD_RGB2_RGB		        0x000000B2			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
															//				 p1: (CR2_rgb2_t) data
#define		CR2CMD_RGB3_RGB		        0x000000B3			// Set RGB tablecode. p0: cam. (0x00, 0x01, 0x02, 0x03)
															//				 p1: table code   (0: rgb0.txt ~ 99: rgb99.txt)
															//				 p2:  iteration count (0: STOP now),    p3: direction (0: normal, 1: reverse)
#define		CR2CMD_RGB4_RGB		        0x000000B4			// Set RGB animation code
															//				 p1: rotation type. (0: STOP, 1: Clockwise, 2: Count-Clockwise)
															//				 p2: animation speed. (0: Fastest ~ 15: Slowest)
															//				 p3: rotation count

//----
#define		CR2CMD_TEST					0x00000100


//---------- Sensor Board Code
#define		CR2SENSORBOARD_N1P1			0x00000011
#define		CR2SENSORBOARD_N1P2			0x00000012
#define		CR2SENSORBOARD_N1PL			0x00000013
#define		CR2SENSORBOARD_N1PL2		0x00000014


//---------- Camera Image Resolution
#define		CR2CAMERA_RESOLUTION_NULL		0x00000000
#define		CR2CAMERA_RESOLUTION_320x240	0x00000001
#define		CR2CAMERA_RESOLUTION_640x480	0x00000002
#define		CR2CAMERA_RESOLUTION_640x240	0x00000003
#define		CR2CAMERA_RESOLUTION_720x480	0x00000004
#define		CR2CAMERA_RESOLUTION_720x240	0x00000005


//---------- Camera Image Color format
#define		CR2CAMERA_IMAGE_RGB					0x00000000
#define		CR2CAMERA_IMAGE_YUV422_PLANAR		0x00000001
#define		CR2CAMERA_IMAGE_YUV422_INTERLACED	0x00000002
#define		CR2CAMERA_IMAGE_YUV420_PLANAR		0x00000003
#define		CR2CAMERA_IMAGE_YUV420_INTERLACED	0x00000004


#define		CR2CAM_STATE_NULL			0x00000000	
#define		CR2CAM_STATE_INITED			0x00000001
#define		CR2CAM_STATE_RESERVED		0x00000002
#define		CR2CAM_STATE_STARTED		0x00000003
	

//-------  2012/0927, for Autotee interface.
#define AUTOTEE_VENDOR_NULL						0
#define AUTOTEE_VENDOR_DEFAULT					(AUTOTEE_VENDOR_NULL)
#define AUTOTEE_VENDOR_JUNGWON_MAGICSHOT		0x0001
#define AUTOTEE_VENDOR_CITYANDTECH				0x0002

#define AUTOTEE_MODEL_NULL						0x0000
#define AUTOTEE_MODEL_DEFAULT					(AUTOTEE_MODEL_NULL)

	// Model number for JUNGWONG Magicshot.
#define AUTOTEE_MODEL_JUNGWON_S50				0x0001
#define AUTOTEE_MODEL_JUNGWON_S100				0x0002
#define AUTOTEE_MODEL_JUNGWON_S185				0x0003
#define AUTOTEE_MODEL_JUNGWON_S125				0x0004

#define AUTOTEE_MODEL_CITYANDTECH_JD001			0x0011

	// Current state of Autotee
#define AUTOTEE_STATE_NULL						0x0000
#define AUTOTEE_STATE_CONNECTED					0x0001
#define AUTOTEE_STATE_NEWDATA					0x0002
#define AUTOTEE_STATE_DISCONNECTED				0x0003
#define AUTOTEE_STATE_ERROR						0x0004

	// Current state of Autotee
#define AUTOTEE_EVENT_NULL						0x0000
#define AUTOTEE_EVENT_HEIGHT					0x0001
#define AUTOTEE_EVENT_KEY						0x0002


//-------  2012/1009, for SwingPlate interface.
#define SWINGPLATE_VENDOR_NULL						0
#define SWINGPLATE_VENDOR_DEFAULT					(AUTOTEE_VENDOR_NULL)
#define SWINGPLATE_VENDOR_JUNGWON				0x0001

#define SWINGPLATE_MODEL_NULL						0x0000
#define SWINGPLATE_MODEL_DEFAULT					(AUTOTEE_MODEL_NULL)

	// Model number for JUNGWONG 
#define SWINGPLATE_MODEL_JUNGWON_1				0x0001

	// Current state of Autotee
#define SWINGPLATE_STATE_NULL						0x0000
#define SWINGPLATE_STATE_CONNECTED					0x0001
#define SWINGPLATE_STATE_BUSY						0x0002
#define SWINGPLATE_STATE_DISCONNECTED				0x0003
#define SWINGPLATE_STATE_ERROR						0x0004

	// Swing Plate Direction code for type1 data.
#define SPDIR_INIT				0
#define SPDIR_DN				1
#define SPDIR_NE				2
#define SPDIR_DE				3
#define SPDIR_SE				4
#define SPDIR_DS				5
#define SPDIR_SW				6
#define SPDIR_DW				7
#define SPDIR_NW				8


#define	FEET2METER(x)	((x)*0.304799999536)

/*----------------------------------------------------------------------------
 	Description	: Type definition of structures and data type
 -----------------------------------------------------------------------------*/
#if defined(_WIN64)
typedef long long PARAM_T;				// 64bit..
#else
typedef signed int PARAM_T; 			// 32bit..
#endif


//---- Basic data type for N1P/N3P project
typedef	  signed char			I08;
typedef	  signed short			I16;
typedef	  signed int			I32;
typedef	  signed long			L32;
typedef	unsigned char			U08;
typedef	unsigned short			U16;
typedef	unsigned int			U32;
typedef	unsigned long			UL32;
typedef void*					HAND;
typedef	long long				I64;
typedef	unsigned long long		U64;
//---- Return code for CR2 API function
typedef		I32					CR2_result_t;

typedef struct _CR2_guid {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} CR2_guid, *PCR2_guid;
	
//---- Shot Result information
typedef		struct _CR2_shotdata {
	int ballspeedX10;
	int clubspeed_BX10;
	int clubspeed_AX10;
	int clubpathX10;
	int clubfaceangleX10;
	int sidespin;
	int backspin;
	int azimuthX10;
	int inclineX10;
} CR2_shotdata_t;

//---- Point data for trajectory
typedef		struct _CR2_point {
	int x;
	int y;
	int z;
	int t;
} CR2_point_t;

//---- Ball position 
typedef		struct _CR2_ballposition {
	int ballexist;				// 0: Don't Exist, 1: Exist
	int shotresult;				// 0: Last shot was NOT me.  1: Last shot was me!!
	int x;						// X position [mm]
	int y;						// Y position [mm]
	int z;						// Z position [mm]
} CR2_ballposition_t;



//---- Point data for trajectory
#define CR2TRAJECTORYMAXCOUNT	1000
typedef		struct _CR2_trajectory {
	int count;
	CR2_point_t pts[CR2TRAJECTORYMAXCOUNT];	// Unit of x, y, z: [cm],   t: [0.001sec == msec]
	int peakheight;							// [m]
	int carrydistance;						// [m]
	int sidedistance;						// [m]
	int flighttimeX1000;					// [msec]
} CR2_trajectory_t;

//---- Velocity and rotation Vector for trajectory
typedef		struct _CR2_vs {
	int vx;				
	int vy;				
	int vz;
	int sidespin;
	int backspin;
	int t;				
} CR2_vs_t;

typedef		struct _CR2_trajectoryEX {
	int count;
	CR2_point_t pts_mm[CR2TRAJECTORYMAXCOUNT];	// Unit of x, y, z: [mm],   t: [0.001sec == msec]
	int peakheight;								// [m]
	int carrydistance;							// [m]
	int sidedistance;							// [m]
	int flighttimeX1000;						// [msec]
	CR2_vs_t vs[CR2TRAJECTORYMAXCOUNT];	// Unit of vx, vy, vz: [mm/s]
                                        //       sidespin, backspin: [rpm]
                                        //       t: [0.001sec == msec]
	int udata[32];								// Reserved.
} CR2_trajectoryEX_t;


typedef		struct _CR2_swingcam {
	int state;		// 0: Unopened. 1: Opended, but NOT started yet. 2: STARTED. -1: Device NOT Supported.
	int device;		// 0: Undecieded. 1: XYVIEW CARD, 2: USB Camera
	int width;		// Image width,  320, 640, 720, ... 
	int height;		// Image height, 240, 480, ... 
	int imgformat;	// 0: Undecided. 1: YUV422, 2: YUV420, 3: RGB24, ...
	int fpsX100;	// Frame per sec * 100.  (330 means 3.3 frame per sec.)
	int rsvd0;		// Reserved. 
	int rsvd1;		// Reserved. 
	int rsvd2;		// Reserved. 
	int rsvd3;		// Reserved. 
} CR2_swingcam_t;



//-------  2012/0927, for Autotee interface.
typedef	struct _CR2_autotee_config {
	//-- Serial Configuration of port.
	//   When portnum is 0, default values are used and followings are NOT used.
	// 		BaudRate, ByteSize, fParity, Parity, StopBit  (See winbase.h)
	int portnum;		// 0: Use default.  1 ~ 255: COM1 ~ COM255
	int baudrate;		// Port speed. CBR_110, CBR_300, ..., CBR256000
	int databitsize;	// Data bit size. 6, 7, 8, ...
	int parity;			// Parity. 0: NOPARITY, 1: ODDPARITY, 2: EVENPARITY,...
	int stopbitsize;	// Stop bit size. 0: ONESTOPBIT, 1: ONE5STOPBITS (==1.5), 2: TWOSTOPBITS

	//-- Vendor/model.
	int vendor;			// 0: Use default.   Others: See definitions of AUTOTEE_VENDOR_???
	int machinemodel;	// 0: Use default.   Others: See definitions of AUTOTEE_MODEL_???
	int vendordata[32];	// Vendor-specific data.
	
	//-- User/additional data. 
	int userdata[32];	// User data. These data are copied and represented in CR2_autotee_data_t.

} CR2_autotee_config_t;


typedef	struct _CR2_autotee_data {
	int state;			// 0: NULL, 1: CONNECTED, 2: NEWDATA, 3: DISCONNECTED, 4: ERROR.
	int heightvalue;	// height value of tee. [mm] unit.

	int data[32];		// Additional data.				// [0]: event code, [1]: eventvalue
	CR2_autotee_config_t acf;	// Modified Autoconfig data. 
} CR2_autotee_data_t;

//-------  2012/1009, for SwingPlate interface.
typedef	struct _CR2_swingplate_config {
	//-- Serial Configuration of port.
	//   When portnum is 0, default values are used and followings are NOT used.
	// 		BaudRate, ByteSize, fParity, Parity, StopBit  (See winbase.h)
	int portnum;		// 0: Use default.  1 ~ 255: COM1 ~ COM255
	int baudrate;		// Port speed. CBR_110, CBR_300, ..., CBR256000
	int databitsize;	// Data bit size. 6, 7, 8, ...
	int parity;			// Parity. 0: NOPARITY, 1: ODDPARITY, 2: EVENPARITY,...
	int stopbitsize;	// Stop bit size. 0: ONESTOPBIT, 1: ONE5STOPBITS (==1.5), 2: TWOSTOPBITS

	//-- Vendor/model.
	int vendor;			// 0: Use default.   Others: See definitions of SWINGPLATE_VENDOR_???
	int machinemodel;	// 0: Use default.   Others: See definitions of SWINGPLATE_MODEL_???
	int vendordata[32];	// Vendor-specific data.
	
} CR2_swingplate_config_t;


typedef	struct _CR2_swingplate_data_type1 {
	int typecode;	// typecode of this data. fixed to '1'
	int dircode;	// Direction code. see SPDIR_XX
	int height;		// height value. [mm] unit.
} CR2_swingplate_data_type1_t;


typedef struct _CR2_serial_t {
	int valid;							// 0: invalid, 1: valid.
	char str[128];						// Null-terminated string
} CR2_serial_t;


typedef struct _CR2_shotdata_full {
	CR2_guid shotguid;					// GUID of shot result

	I32 goodshot;						// 1: Good shot, 0: NO shot.
	I32	ballspeedx1000;					// Ball speed	[mm/s]
	I32	azimuthx1000;					// Ball azimuth	[(1/1000) degree]
	I32	inclinex1000;					// Ball incline	[(1/1000) degree]

	I32	spindata_kind;					// 0: ball-marking result.  1: ball-club result	
	I32	sidespin;						// side spin. (+: slice, -: hook)	[rpm]
	I32	backspin;						// back spin (+: backspin, -: Topspin)	[rpm]
	I32	rollspin;						// Roll spin. (+: clock-wise, -: Counter clock-wize)	[rpm]

	I32	clubdatakind;					// 0: ball-marking result.  1: ball-club result	
	I32	clubspeed_Bx1000;				// Club speed Before shot	[mm/s]
	I32	clubspeed_Ax1000;				// Club speed After shot	[mm/s]
	I32	clubpathx1000;					// Club path. (+: right,  -: left)	[(1/1000) degree]
	I32	clubfaceanglex1000;				// Club face angle (+: right, -: left)	[(1/1000) degree]
	I32	clubfacelengthx1000;			// Club face length (0: discard this value)	[mm]
	I32	clubballhitpointx1000;			// Club ball hit point from club toe	[mm]
	I32	rsvd4;							// Reserved.  0x00000000	

	I32	mk_assurance;					// marking ball result, data quality assurance. 0 ~ 99
	I32	mk_sidespin;					// marking ball result: side spin. (+: slice, -: hook)	[rpm]
	I32	mk_backspin;					// marking ball result: back spin. (+: backspin, -: Topspin)	[rpm]
	I32	mk_rollspin;					// marking ball result: Roll spin. (+: clock-wise, -: Counter clock-wize)	[rpm]
	I32	mk_spinmagnitude;				// marking ball result: spin magnitude	
	I32	mk_axis_x_x1000000;				// marking ball result: spin axis vector, x value * 1000000
	I32	mk_axis_y_x1000000;				// marking ball result: spin axis vector, y value * 1000000	
	I32	mk_axis_z_x1000000;				// marking ball result: spin axis vector, z value * 1000000	
	I32	mk_clubspeed_Bx1000;			// marking ball result: Club speed Before shot	[mm/s]
	I32	mk_clubspeed_Ax1000;			// marking ball result: Club speed After shot	[mm/s]
	I32	mk_clubpathx1000;				// marking ball result: Club path. (+: right,  -: left)	[(1/1000) degree]
	I32	mk_clubfaceanglex1000;			// marking ball result: Club face angle (+: right, -: left)	[(1/1000) degree]

	I32	bc_assurance;					// Club - Ball result: data quality assurance. 0 ~ 99
	I32	bc_sidespin;					// Club - Ball result: side spin. (+: slice, -: hook)	[rpm]
	I32	bc_backspin;					// Club - Ball result: back spin (+: backspin, -: Topspin)	[rpm]
	I32	bc_rollspin;					// Club - Ball result: Fixed to 0.	[rpm]

	I32	bc_spinmagnitude;				// Club - Ball result: spin magnitude
	I32	bc_axis_x_x1000000;				// Club - Ball result: spin axis vector, x value * 1000000	
	I32	bc_axis_y_x1000000;				// Club - Ball result: spin axis vector. Fixed to 0x00000000	
	I32	bc_axis_z_x1000000;				// Club - Ball result: spin axis vector, z value * 1000000	
	I32	bc_clubspeed_Bx1000;			// Club - Ball result: Club speed Before shot	[mm/s]
	I32	bc_clubspeed_Ax1000;			// Club - Ball result: Club speed After  shot	[mm/s]
	I32	bc_clubpathx1000;				// Club - Ball result: Club path. (+: right,  -: left)	[(1/1000) degree]
	I32	bc_clubfaceanglex1000;			// Club - Ball result: Club face angle (+: right, -: left)	[(1/1000) degree]
	I32	bc_clubfacelengthx1000;			// Club face length (0: discard this value)	[mm]
	I32	bc_clubballhitpointx1000;		// Club ball hit point from club toe	[mm]

	I32	datapad[18];					// Data padding.   Fill with 0x00000000	
} CR2_shotdata_full_t;

typedef		struct _CR_shotdataEX {
	I32 ballspeedx1000;					// [mm/s]
	I32 sidespin;						// [rpm]
	I32 backspin;						// [rpm]
	I32 azimuthx1000;					// [(1/1000) degree]
	I32 inclinex1000;					// [(1/1000) degree]

	I32 clubspeed_Bx1000;				// [mm/s]
	I32 clubspeed_Ax1000;				// [mm/s]
	I32 clubpathx1000;					// [(1/1000) degree]	
	I32 clubfaceanglex1000;				// [(1/1000) degree]
	
	I32 hitheightx1000;					// [mm] Discard it.
} CR_shotdataEX_t;


typedef		struct _CR2_shotdata_ballEx {
	U32    valid;			// 0: result invalid or empty.  1: Valid.
	U32	   reserved;		// 0. reserved.
	double incline;			// Ball incline, [degree]
	double azimuth;			// Ball azimuth, [degree]
	double vmag;			// Ball speed, [m/s]
	double shotAssurance;	// Measurement assurance of speed, incline and azimuth
	double spinmag;			// Ball spin magnitude, [rpm: Round-per-Minute]
	double spinaxis[3];		// Ball spin axis,  [0]: x, [1]: y, [2]: z
	double spinAssurance;	// Measurement assurance of spin
} CR2_shotdata_ballEx_t;

typedef		struct CR2_shotdataEX1_t {
	CR2_guid shotguid;				// GUID of shot

	int category;					// Sensor category
	int rightlefthanded;			// 0: right-haned, 1: left-handed
	int xposx1000;					// Shot ball x position [mm]
	int yposx1000;					// Shot ball y position [mm]
	int zposx1000;					// Shot ball z position [mm]

	int ballspeedx1000;				// ball speed. [m/s] * 1000 = [mm/s]
	int inclineX1000;				// ball incline. [degree] * 1000
	int azimuthX1000;				// ball azimuth. [degree] * 1000

	int spincalc_method;			// Spin calc. method.  0: Using Ball/Club path. 1: Ball Marking based. 2: Dimple based.
	int assurance_spin;				// Spin calcuration assurance. 0-100
	int backspinX1000;				// backspin. [rpm] * 1000
	int sidespinX1000;				// sidespin. [rpm] * 1000
	int rollspinX1000;				// rollspin. [rpm] * 1000

	int clubcalc_method;			// club calc. method.  0: marking-less method, 1: another marking-less method, 2: bar-marking, 3: bar-marking and dot-marking
	int assurance_clubspeed;		
	int assurance_clubpath;
	int assurance_faceangle;
	int assurance_attackangle;
	int assurance_loftangle;
	int assurance_lieangle;
	int assurance_faceimpactLateral;
	int assurance_faceimpactVertical;
	int clubspeedX1000;				// club speed. [m/s] * 1000
	int clubpathX1000;				// club path angle. [degree] * 1000
	int faceangleX1000;				// face angle. [degree] * 1000
	int attackangleX1000;			// attack angle. [degree] * 1000
	int loftangleX1000;				// loftangle. [degree] * 1000
	int lieangleX1000;				// lie angle. [degree] * 1000
	int faceimpactLateralX1000;		// horizontal deviation of impact point from club center. [mm]
	int faceimpactVerticalX1000;	// vertical deviation of impact point from club center. [mm]

	int data[30];					// padding data. 
} CR2_shotdataEX1_t;

#define CR2_SHOTDATA1_MAXSIZE		(256)				// maximum size of shotdata: 256 byte.. :P  

typedef	struct _CR2_rgb2 {
	U08 r[12];
	U08 g[12];
	U08 b[12];
} CR2_rgb2_t;

//---- Call back fucntion for shot event
#if defined(_WIN64)
typedef int (CALLBACK CR2_CALLBACKFUNC) (HAND h, U32 status, CR2_shotdata_t *psd, I64 userparam);
#else
typedef int (CALLBACK CR2_CALLBACKFUNC) (HAND h, U32 status, CR2_shotdata_t *psd, I32 userparam);
#endif

#if defined(_WIN64)
typedef int (CALLBACK CR2_CALLBACKFUNC1) (HAND h, U32 status, HAND hsd, U32 cbfuncid, I64 userparam);
#else
typedef int (CALLBACK CR2_CALLBACKFUNC1) (HAND h, U32 status, HAND hsd, U32 cbfuncid, I32 userparam);
#endif



//---- Call back fucntion for Image capture
typedef int (CALLBACK CR2_CAM_CALLBACKFUNC) (HAND h, I32 chnum, U08 *pimg, I32 len);

/*----------------------------------------------------------------------------
 	Description	: static variable declaration  
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 	Description	: external and internal global variable
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *	Description	: declares the dll function prototype 
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *	Description	: declares the external function prototype 
 -----------------------------------------------------------------------------*/
CR_CR2ADAPT_EXPORT HAND CR2_init(U32 sensorcode, U32 sensornum, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);
CR_CR2ADAPT_EXPORT CR2_result_t CR2_delete(HAND h);
CR_CR2ADAPT_EXPORT CR2_result_t CR2_command(HAND h, U32 cmd, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);


#if defined (__cplusplus)
}
#endif


#endif		// _CR_CR2ADAPT_




