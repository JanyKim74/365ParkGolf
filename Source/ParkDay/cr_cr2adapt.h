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
#define		CR2_OK							0x00000000

#define		CR2_ERR_GENERAL					0x80000000
#define		CR2_ERR_BADHANDLE				0x80000001
#define 	CR2_ERR_DISCONNECT				0x80000002
#define		CR2_ERR_BUSY					0x80000003
#define 	CR2_ERR_NEEDTOINIT				0x80000004
#define 	CR2_ERR_NEEDTOCONFIG			0x80000005
#define 	CR2_ERR_ACTIVATION				0x80000006
#define 	CR2_ERR_APPCODE					0x80000007
#define		CR2_ERR_UNSUPPORTED_CMD			0x80000010
#define		CR2_ERR_BADPARAM				0x80000011
#define		CR2_ERR_CAM_RESERVE_FAIL		0x80000020
#define		CR2_ERR_CAM_NOT_OWNER			0x80000021
#define		CR2_ERR_CAM_CONFIG_FAIL			0x80000022
#define		CR2_ERR_LOGREPORT_FAIL			0x80000023
#define		CR2_ERR_LOGREPORT_FAIL_SUSB		0x80000024
#define		CR2_ERR_LOGREPORT_FAIL_SBACKUP	0x80000025
#define		CR2_ERR_LICENSE_FILE_INVALID	0x80000030
#define		CR2_ERR_LICENSE_DECRYPT_FAIL	0x80000031
#define		CR2_ERR_LICENSE_VERIFY_FAIL		0x80000032

#define		CR2_IS_RESULT_ERROR(x)	(CR2_ERR_GENERAL & (x))


//---------- Sensor Status
#define		CR2STATUS_NULL				0x00000000
#define		CR2STATUS_READY				0x00000001
#define		CR2STATUS_GOODSHOT			0x00000010
#define		CR2STATUS_TRIALSHOT			0x00000011
#define		CR2STATUS_SHOTIMG			0x00000012
#define		CR2STATUS_STATECHANGED		0x00000013
#define		CR2STATUS_SHOTDETECT		0x00000014
#define		CR2STATUS_SHOTSCDATA		0x00000015
#define		CR2STATUS_DISCONNECT		0x00000100
#define		CR2STATUS_BIGSHADOW			0x00000101
#define		CR2STATUS_NOBALL			0x00000102
#define		CR2STATUS_POWERSAVE			0x00000103
#define		CR2STATUS_SLEEP				0x00000104
#define		CR2STATUS_TARGETALIGN		0x00000105

#define		CR2STATUS_LOGREPORT_END		0x00000106

#define		CR2STATUS_NOT_STARTED		0x00000106 // duplicated with CR2STATUS_LOGREPORT_END, but the usage is different, so keep current values
#define		CR2STATUS_NOT_ACTIVED		0x00000107

#define		CR2STATUS_POWERON			0x00000108
#define		CR2STATUS_POWEROFF			0x00000109

#define		CR2STATUS_REBOOT			0x00000110 // not used

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
#define		CR2CMD_DLLVERSION2			0x00000004
#define		CR2CMD_OPERATION_START		0x00000010				// p0: callbackfuncion.   p1; user param.
#define		CR2CMD_OPERATION_STOP		0x00000011
#define		CR2CMD_OPERATION_RESTART	0x00000012
#define		CR2CMD_OPERATION_ACTIVATE	0x00000013				// p0, 0: No operation,   1: Normal operation
#define		CR2CMD_OPERATION_START1		0x00000014				// p0: callbackfuncion1.   p1: callbackfunction1_id p1; user param.
#define		CR2CMD_OPERATION_START2		0x00000015				// p0: callbackfuncion2.   p1; user param.
#define		CR2CMD_USETEE				0x00000020
#define		CR2CMD_USECLUB				0x00000021
#define		CR2CMD_WIND					0x00000022
#define		CR2CMD_SETTEESTATE			0x00000023
#define		CR2CMD_SETRIGHTLEFT			0x00000024				// p0, 0: Right-handed sensor, 1: Left-handed sensor
#define		CR2CMD_SETUNIT				0x00000025				// p0: distance.    p1: speed

#define		CR2CMD_SENSORSTATUS			0x00000030
#define		CR2CMD_SENSORSTATUS2		0x00000031
#define		CR2CMD_AREAALLOW			0x00000032
#define		CR2CMD_BALLPOSITION			0x00000033
#define		CR2CMD_SHOTRESULTEX			0x00000034				// 	p0, 0: Don't clear result, 1: Clear result
																//  p1: CR2_shotdata_ballEx_t
#define		CR2CMD_BALLPOSITION3D		0x00000035				//  20200730
#define		CR2CMD_SHOTRESULTFULL		0x00000036				// 	p0: CR2_shotdata_full_t
#define		CR2CMD_NOBALLPOSITION		0x00000037				// 	p0: number of the no ball. p1: position of the no ball.
#define		CR2CMD_SENSORSTATUS3		0x00000038				// p0: sensor run-status.   p1; sensor readystatus
#define		CR2CMD_CALC_TRAJECTORY		0x00000040
#define		CR2CMD_SET_AIRPRESSURE_PA	0x00000041				// p0: air pressure in Pa, not applicable for portable sensor(e.g. EYEMINI)
#define		CR2CMD_SET_ALTITUDE			0x00000042				// p0: altitutde in m, not applicable for portable sensor(e.g. EYEMINI)
#define		CR2CMD_SET_CLUBMODE			0x00000043				// p0: club mode.   0: Default. 1: NO Club CALC. 2: NO Club Marking, 3: Club Marking #1, 4: Club Marking #2
#define		CR2CMD_AREAINFORM_GET		0x00000044
			// p0: *enabled	(0: Trouble Mat Not enabled, 1: enabled)
			// p1: (CR2_area_t *)&AreaTeeIronPutterClub[0].   AreaTeeIronPutterClub[0]: Driver Area, AreaTeeIronPutterClub[1]: Iron Area,
			//                                                  AreaTeeIronPutterClub[2]: Putter Area, AreaTeeIronPutterClub[3]: Club valid Area
			// p2: (CR2_area_t *)&pTroubleArea[0]. pTroubleArea[0]: Rough area,  pTroubleArea[1]: Bunker area.
			// p3: 0
#define		CR2CMD_READYAREA_SET		0x00000045				// p0: areakind (see READYAREA_AREAKIND_CLEAR, ..., READYAREA_AREAKIND_READYAREA),  p1: CR2_area_t *pReadyArea
#define		CR2CMD_GET_ALTITUDE			0x00000046				// p0: altitutde in m

#define		CR2CMD_SET_CALLBACK			0x00000048				// p0: new callback func. p1: callback function id    20200513

#define		CR2CMD_CALC_TRAJECTORY_SIMPLE	0x00000049

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
#define		CR2CMD_SHOTDATAEX1			0x00000093			// Get Extended Shot club data

//----		2018/0531
#define		CR2CMD_RESETSENSOR			0x00000098			// RESET and REBOOT SENSOR board
#define		CR2CMD_CLEARLAN				0x00000099			// RESET and CLEAR LAN

//----		2019/0508
#define		CR2CMD_GETSWSPEC			0x000000A0			// p0: U08 swspec[64+4]
#define		CR2CMD_GETBARCODE			0x000000A1			// p0: U08 barcode[64]
#define		CR2CMD_SETCURRENTDIRSTR		0x000000A2			// p0: TCHAR currentdir[1024]
#define 	CR2CMD_SET_HOSTIPADDR		0x000000A3			// p0: CHAR ipaddr[32]
#define 	CR2CMD_GET_HOSTIPADDR		0x000000A4
//----		2023/1227
#define		CR2CMD_GET_CALIDATA			0x000000A5			// p0: U08 calidata[64*1023], p1: U32 calilength

//----		2020/0331
#define		CR2CMD_RGB0_RGB		        0x000000B0			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both.
															//				 p1: (((R << 16) & 0x00FF0000) | ((G << 8) & 0x0000FF00) | (B & 0x000000FF))
#define		CR2CMD_RGB1_RGB		        0x000000B1			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both.
															//				 p1: (((R << 16) & 0x00FF0000) | ((G << 8) & 0x0000FF00) | (B & 0x000000FF))
															//               p2: ledid (0 ~ 11)
#define		CR2CMD_RGB2_RGB		        0x000000B2			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both.
															//				 p1: (CR2_rgb2_t) data
#define		CR2CMD_RGB3_RGB		        0x000000B3			// Set RGB tablecode. p0: cam. (0x00, 0x01, 0x02, 0x03)
															//				 p1: table code   (0: rgb0.txt ~ 99: rgb99.txt)
															//				 p2:  iteration count (0: STOP now),    p3: direction (0: normal, 1: reverse)
#define		CR2CMD_RGB4_RGB		        0x000000B4			// Set RGB animation code
															//				 p1: rotation type. (0: STOP, 1: Clockwise, 2: Count-Clockwise)
															//				 p2: animation speed. (0: Fastest ~ 15: Slowest)

/*** Portable Platform Command Extention *****************/
/* Setting / Config */
#define		CR2CMD_GET_STATEDATA			0x000000B8
#define		CR2CMD_SET_AIRPRESSURE_PA2		0x000000B9		// p0: air pressure in x1000 Pa
#define		CR2CMD_SET_ALTITUDE2			0x000000BA		// p0: altitutde in x1000 m, p1: forced flag
#define		CR2CMD_SET_UNIXTIME				0x000000BB
#define		CR2CMD_GET_RIGHTLEFT			0x000000C0
#define		CR2CMD_GET_CLUB					0x000000C1			 /* Putter, Iron, Driver */
#define		CR2CMD_GET_ALTITUDE_MODE		0x000000C2
#define		CR2CMD_GET_ALTITUDE_VALUE		0x000000C3

/* System */
#define 	CR2CMD_GET_SYSTEM_INFO			0x000000C4
#define		CR2CMD_CHECK_UPGRADE			0x000000C5
#define		CR2CMD_START_UPGRADE			0x000000C6
#define		CR2CMD_SET_SYSTEMTIME			0x000000C7
#define		CR2CMD_TRIGGER_SW_UPGRADE		0x000000C8

/* Crash */
#define		CR2CMD_MAKE_CRASH				0x000000C9

/* WiFi */
#define		CR2CMD_SET_WIFI					0x000000CA
#define		CR2CMD_GET_WIFI					0x000000CB
#define		CR2CMD_SET_WIFILIST				0x000000CC
#define		CR2CMD_GET_WIFILIST				0x000000CD
#define		CR2CMD_GET_APINFO				0x000000CE
#define		CR2CMD_DISCONNECT_WIFI			0x000000CF

/* Get Sensing values */
#define		CR2CMD_GET_GPS					0x000000D0
#define		CR2CMD_GET_AIRPRESSURE			0x000000D1
#define		CR2CMD_GET_GRADIENT				0x000000D2



/* License / Security */
#define		CR2CMD_SECURE_FACTORY_REGISTER			0x000000D3
#define		CR2CMD_SECURE_CHECK_UPDATE				0x000000D4
#define		CR2CMD_SECURE_LICENSE_ACTIVATE			0x000000D5
#define		CR2CMD_SECURE_CLIENT_PAIRING			0x000000D6
#define		CR2CMD_SECURE_CLIENT_UNPAIRING			0x000000D7
#define		CR2CMD_SECURE_SET_SYSTEMTIME			0x000000D8
#define		CR2CMD_SECURE_GET_ACT_CODE				0x000000D9

#define		CR2CMD_SET_TEST_LICENSE					0x000000DA
#define		CR2CMD_SET_SLEEPMODE					0x000000DB
#define		CR2CMD_POWER_OFF						0x000000DC

#define		CR2CMD_SECURE_LICENSE_UPDATE			0x000000DD
#define		CR2CMD_SECURE_ENC_SN					0x000000DE
#define		CR2CMD_REBOOT_SENSOR					0x000000DF

/* Etc */
#define		CR2CMD_REQUEST_SHOT_DATA				0x000000E0
#define		CR2CMD_ALIGNMENT_OPERATION				0x000000E1
#define		CR2CMD_ALIGNMENT_SET_STATUS				0x000000E2
#define		CR2CMD_ALIGNMENT_GET_STATUS				0x000000E3
#define		CR2CMD_ALIGNMENT_APPLY					0x000000E4
#define		CR2CMD_ALIGNMENT_GET_DIRECTION_DATA		0x000000E5
#define		CR2CMD_ALIGNMENT_GET_POSITION_DATA		0x000000E6
#define		CR2CMD_REQUEST_LOGREPORT				0x000000E7
#define		CR2CMD_CHANGE_MPTOOL_MODE				0x000000E8
#define		CR2CMD_CHANGE_APP_MODE					0x000000E9
#define		CR2CMD_SET_SHOTRESULT					0x000000EA
#define     CR2CMD_SET_HIDECARRY        			0x000000EB                              // p0: hide carry setting.      0: Not hide,   1: Hide carry distance.
#define		CR2CMD_HIDE_SHOTDATA					0x000000EC
#define		CR2CMD_SET_SEPARATED_READY_AREA			0x000000ED
#define		CR2CMD_GET_SEPARATED_READY_AREA			0x000000EE
#define 	CR2CMD_GET_SENSOR_IDLE_STATUS			0x000000EF

/* Device Sensor Status CHECK */
#define		CR2CMD_CHECK_WIFI						0x000000F0
#define		CR2CMD_CHECK_GPS						0x000000F1
#define		CR2CMD_CHECK_USB						0x000000F2
#define		CR2CMD_CHECK_IMU						0x000000F3
#define		CR2CMD_CHECK_POWER						0x000000F4
#define		CR2CMD_CHECK_PA							0x000000F5
#define		CR2CMD_CHECK_CALI						0x000000F6
#define		CR2CMD_CHECK_SERVER						0x000000F7
#define		CR2CMD_CHECK_FPGA						0x000000F8

/* Game Mode */
#define		CR2CMD_SET_HOSTMODE						0x000001E0
#define		CR2CMD_GET_HOSTMODE						0x000001E1
#define 	CR2CMD_GET_AREAIRON						0x000001E2

/* COM Server */
#define		CR2CMD_SECURE_RECV_CHECK_UPDATE			0x00000200
#define		CR2CMD_SECURE_RECV_LICENSE_ACTIVATE		0x00000201

//----
#define		CR2CMD_TEST					0x00000100
#define		CR2CMD_FUNC_CALL			0x00000101
#define     CR2CMD_READYSIMULATE 		0x00000102 // for windows ci test ready smulation
#define     CR2CMD_GET_READYSIMULATE_RESULT 	 0x00000103 // for windows ci test ready smulation
#define     CR2CMD_GET_READY_STATUS 0x00000104 // for windows ci test ready smulation


/********************************************************/
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

//---------- Unit display
#define CR2UNIT_MIX_YARD_MPH	0x0000	// old-style (deprecated)
#define CR2UNIT_MIX_METER_MS	0x0001	// old-style (deprecated)
#define CR2UNIT_MIX_YARD_MS		0x0002	// old-style (deprecated)
#define CR2UNIT_MIX_METER_MPH	0x0003	// old-style (deprecated)
#define CR2UNIT_DISTANCE_YARD	0x0010
#define CR2UNIT_DISTANCE_METER	0x0011
#define CR2UNIT_SPEED_MPH		0x0020
#define CR2UNIT_SPEED_MS		0x0021
#define CR2UNIT_SPEED_KMH		0x0022
#define CR2UNIT_SPEED_YDS		0x0023


//---------- Unit convert
#define KMH2MS(x)			((x) / 3.6)
#define MS2KMH(x)			((x) * 3.6)
#define MS2MPH(x)			((x) * 2.23694)

#define YARD2METER(x)		((x) / 0.9144)
#define METER2YARD(x)		((x) * 1.0936132983)

#define	FEET2METER(x)		((x) * 0.304799999536)
#define	METER2FEET(x)		((x) * 3.2808399)
#define METER2FEET_INT(x)	((int)(METER2FEET(x)+0.005))

#define READYAREA_AREAKIND_CLEAR				(0x00)
#define READYAREA_AREAKIND_NON_TROUBLE_AREA		(0x01)
#define READYAREA_AREAKIND_ROUGH_AREA			(0x02)
#define READYAREA_AREAKIND_BUNKER_AREA			(0x04)
#define READYAREA_AREAKIND_READYAREA			(0x10)				// use p1, CR2_area_t *pAreaArea

/*----------------------------------------------------------------------------
	Description	: Type definition of structures and data type
 -----------------------------------------------------------------------------*/
 //---- Basic data type for N1P/N3P project
	typedef char				I08;
	typedef short				I16;
	typedef int					I32;
	typedef long long			I64;
	typedef unsigned char       U08;
	typedef unsigned short      U16;
	typedef unsigned int        U32;
	typedef unsigned long long  U64;
	typedef void* HAND;

#ifndef PARAM_T
#if defined(_WIN64)
	typedef	I64 PARAM_T;		// 64bit..
#else
	typedef	I32 PARAM_T;		// 32bit..
#endif
#endif

	//---- Return code for CR2 API function
	typedef	I32					CR2_result_t;

	typedef struct _CR2_guid {
		ULONG	Data1;
		U16		Data2;
		U16		Data3;
		U08		Data4[8];
	} CR2_guid, * PCR2_guid;

	//---- Shot Result information
	typedef		struct _CR2_shotdata {
		I32 ballspeedX10;
		I32 clubspeed_BX10;
		I32 clubspeed_AX10;
		I32 clubpathX10;
		I32 clubfaceangleX10;
		I32 sidespin;
		I32 backspin;
		I32 azimuthX10;
		I32 inclineX10;
	} CR2_shotdata_t;

	//---- Point data for trajectory
	typedef		struct _CR2_point {
		I32 x;
		I32 y;
		I32 z;
		I32 t;
	} CR2_point_t;

	typedef struct _CR2_point_float {
		double x;
		double y;
		double z;
	} CR2_point_float_t;

	//---- Ball position
	typedef		struct _CR2_ballposition {
		I32 ballexist;				// 0: Don't Exist, 1: Exist
		I32 shotresult;				// 0: Last shot was NOT me.  1: Last shot was me!!
		I32 x;						// X position [mm]
		I32 y;						// Y position [mm]
		I32 z;						// Z position [mm]
	} CR2_ballposition_t;

	typedef struct _CR2_readyresult {
		U32 readyResult;			// 0: Not Ready, 1: Ready
		I32 padding;				// Padding
		double readyBallRP[2];		// Ready Ball RP
		// double  [2];	// Ready Ball Position
		CR2_point_float_t readyBallPosP[2];	// Ready Ball Position pixel position
		CR2_point_float_t readyBallPosB;	// Ready Ball Position world position
		double reserved[8];			// Reserved
	} CR2_readyresult_t;


	//---- Point data for trajectory
#define CR2TRAJECTORYMAXCOUNT	1000
	typedef		struct _CR2_trajectory {
		I32 count;
		CR2_point_t pts[CR2TRAJECTORYMAXCOUNT];	// Unit of x, y, z: [cm],   t: [0.001sec == msec]
		I32 peakheight;							// [m]
		I32 carrydistance;						// [m]
		I32 sidedistance;						// [m]
		I32 flighttimeX1000;					// [msec]
	} CR2_trajectory_t;

	//---- Velocity and rotation Vector for trajectory
	typedef		struct _CR2_vs {
		I32 vx;
		I32 vy;
		I32 vz;
		I32 sidespin;
		I32 backspin;
		I32 t;
	} CR2_vs_t;

	typedef		struct _CR2_trajectoryEX {
		I32 count;
		CR2_point_t pts_mm[CR2TRAJECTORYMAXCOUNT];	// Unit of x, y, z: [mm],   t: [0.001sec == msec]
		I32 peakheight;								// [m]
		I32 carrydistance;							// [m]
		I32 sidedistance;							// [m]
		I32 flighttimeX1000;						// [msec]
		CR2_vs_t vs[CR2TRAJECTORYMAXCOUNT];	// Unit of vx, vy, vz: [mm/s]
		//       sidespin, backspin: [rpm]
		//       t: [0.001sec == msec]
		I32 carrydistanceX1000;
		I32 udata[31];								// Reserved.
	} CR2_trajectoryEX_t;


	typedef		struct _CR2_swingcam {
		I32 state;		// 0: Unopened. 1: Opended, but NOT started yet. 2: STARTED. -1: Device NOT Supported.
		I32 device;		// 0: Undecieded. 1: XYVIEW CARD, 2: USB Camera
		I32 width;		// Image width,  320, 640, 720, ...
		I32 height;		// Image height, 240, 480, ...
		I32 imgformat;	// 0: Undecided. 1: YUV422, 2: YUV420, 3: RGB24, ...
		I32 fpsX100;	// Frame per sec * 100.  (330 means 3.3 frame per sec.)
		I32 rsvd0;		// Reserved.
		I32 rsvd1;		// Reserved.
		I32 rsvd2;		// Reserved.
		I32 rsvd3;		// Reserved.
	} CR2_swingcam_t;



	//-------  2012/0927, for Autotee interface.
	typedef	struct _CR2_autotee_config {
		//-- Serial Configuration of port.
		//   When portnum is 0, default values are used and followings are NOT used.
		// 		BaudRate, ByteSize, fParity, Parity, StopBit  (See winbase.h)
		I32 portnum;		// 0: Use default.  1 ~ 255: COM1 ~ COM255
		I32 baudrate;		// Port speed. CBR_110, CBR_300, ..., CBR256000
		I32 databitsize;	// Data bit size. 6, 7, 8, ...
		I32 parity;			// Parity. 0: NOPARITY, 1: ODDPARITY, 2: EVENPARITY,...
		I32 stopbitsize;	// Stop bit size. 0: ONESTOPBIT, 1: ONE5STOPBITS (==1.5), 2: TWOSTOPBITS

		//-- Vendor/model.
		I32 vendor;			// 0: Use default.   Others: See definitions of AUTOTEE_VENDOR_???
		I32 machinemodel;	// 0: Use default.   Others: See definitions of AUTOTEE_MODEL_???
		I32 vendordata[32];	// Vendor-specific data.

		//-- User/additional data.
		I32 userdata[32];	// User data. These data are copied and represented in CR2_autotee_data_t.

	} CR2_autotee_config_t;


	typedef	struct _CR2_autotee_data {
		I32 state;			// 0: NULL, 1: CONNECTED, 2: NEWDATA, 3: DISCONNECTED, 4: ERROR.
		I32 heightvalue;	// height value of tee. [mm] unit.

		I32 data[32];		// Additional data.				// [0]: event code, [1]: eventvalue
		CR2_autotee_config_t acf;	// Modified Autoconfig data.
	} CR2_autotee_data_t;

	//-------  2012/1009, for SwingPlate interface.
	typedef	struct _CR2_swingplate_config {
		//-- Serial Configuration of port.
		//   When portnum is 0, default values are used and followings are NOT used.
		// 		BaudRate, ByteSize, fParity, Parity, StopBit  (See winbase.h)
		I32 portnum;		// 0: Use default.  1 ~ 255: COM1 ~ COM255
		I32 baudrate;		// Port speed. CBR_110, CBR_300, ..., CBR256000
		I32 databitsize;	// Data bit size. 6, 7, 8, ...
		I32 parity;			// Parity. 0: NOPARITY, 1: ODDPARITY, 2: EVENPARITY,...
		I32 stopbitsize;	// Stop bit size. 0: ONESTOPBIT, 1: ONE5STOPBITS (==1.5), 2: TWOSTOPBITS

		//-- Vendor/model.
		I32 vendor;			// 0: Use default.   Others: See definitions of SWINGPLATE_VENDOR_???
		I32 machinemodel;	// 0: Use default.   Others: See definitions of SWINGPLATE_MODEL_???
		I32 vendordata[32];	// Vendor-specific data.

	} CR2_swingplate_config_t;


	typedef	struct _CR2_swingplate_data_type1 {
		I32 typecode;	// typecode of this data. fixed to '1'
		I32 dircode;	// Direction code. see SPDIR_XX
		I32 height;		// height value. [mm] unit.
	} CR2_swingplate_data_type1_t;


	typedef struct _CR2_serial_t {
		I32		valid;							// 0: invalid, 1: valid.
		char	str[128];						// Null-terminated string
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

		I32 category;					// Sensor category
		I32 rightlefthanded;			// 0: right-haned, 1: left-handed
		I32 xposx1000;					// Shot ball x position [mm]
		I32 yposx1000;					// Shot ball y position [mm]
		I32 zposx1000;					// Shot ball z position [mm]

		I32 ballspeedx1000;				// ball speed. [m/s] * 1000 = [mm/s]
		I32 inclineX1000;				// ball incline. [degree] * 1000
		I32 azimuthX1000;				// ball azimuth. [degree] * 1000

		I32 spincalc_method;			// Spin calc. method.  0: Using Ball/Club path. 1: Ball Marking based. 2: Dimple based.
		I32 assurance_spin;				// Spin calcuration assurance. 0-100
		I32 backspinX1000;				// backspin. [rpm] * 1000
		I32 sidespinX1000;				// sidespin. [rpm] * 1000
		I32 rollspinX1000;				// rollspin. [rpm] * 1000

		I32 clubcalc_method;			// club calc. method.  0: marking-less method, 1: another marking-less method, 2: bar-marking, 3: bar-marking and dot-marking
		I32 assurance_clubspeed;
		I32 assurance_clubpath;
		I32 assurance_faceangle;
		I32 assurance_attackangle;
		I32 assurance_loftangle;
		I32 assurance_lieangle;
		I32 assurance_faceimpactLateral;
		I32 assurance_faceimpactVertical;
		I32 clubspeedX1000;				// club speed. [m/s] * 1000
		I32 clubpathX1000;				// club path angle. [degree] * 1000
		I32 faceangleX1000;				// face angle. [degree] * 1000
		I32 attackangleX1000;			// attack angle. [degree] * 1000
		I32 loftangleX1000;				// loftangle. [degree] * 1000
		I32 lieangleX1000;				// lie angle. [degree] * 1000
		I32 faceimpactLateralX1000;		// horizontal deviation of impact point from club center. [mm]
		I32 faceimpactVerticalX1000;	// vertical deviation of impact point from club center. [mm]

		I32	spinMag2DX1000;				// spinMag2D * 1000
		I32 spinAxis2Dx1000;			// spinAxis2D * 1000

		U64 impactTimestamp;

		I32 data[26];					// padding data.
	} CR2_shotdataEX1_t;

	typedef		struct _CR_syncShotdata {
		CR2_guid shotguid;				// GUID of shot
		I32 carryx10;					// [cm]

		I32 additional_flag;	//additional shotdata 1: use, 0: not use
		I32 ballspeedx10;					// [cm/s]
		I32 sidespin;						// [rpm]
		I32 backspin;						// [rpm]
		I32 azimuthx10;					// [(1/1000) degree]
		I32 inclinex10;					// [(1/1000) degree]

		I32 clubspeed_Bx10;				// [cm/s]
		I32 clubspeed_Ax10;				// [cm/s]
		I32 clubpathx10;					// [(1/1000) degree]
		I32 clubattackanglex10;				// [(1/1000) degree]
	} CR_syncShotdata_t;

#define CR2_SHOTDATA1_MAXSIZE		(256)				// maximum size of shotdata: 256 byte.. :P

	typedef	struct _CR2_rgb2 {
		U08 r[12];
		U08 g[12];
		U08 b[12];
	} CR2_rgb2_t;


	typedef    struct _CR2_area {			// 2022/1128
		I32 sx;    // start x  [mm]
		I32 sy;    // start y [mm]
		I32 ex;    // end x [mm]
		I32 ey;    // endt y [mm]
	} CR2_area_t;

	typedef		struct _CR2_statedata_t {
		U32     rightleft;

		U32     clubtype;
		U32     allowTee;
		U32     allowIron;
		U32     allowPutter;

		U32		unitDistance;
		U32		unitSpeed;

		double  altitude;
		U32		altitude_mode;	// 0: auto, 1: manual
	} CR2_statedata_t;

	typedef		enum _CR2_poweroff_reason_codes_t {
		CR2_POWEROFF_CODES_HALT_BUTTON_PRESSED = 0x0000,
		CR2_POWEROFF_CODES_HALT_AUTO_SHUTDOWN,
		CR2_POWEROFF_CODES_HALT_LOW_BATTERY,
		CR2_POWEROFF_CODES_HALT_CRCMD,
		CR2_POWEROFF_CODES_HALT_MAX,
		CR2_POWEROFF_CODES_REBOOT_UPGRADE = 0x0100,
		CR2_POWEROFF_CODES_REBOOT_WATCHDOG,
		CR2_POWEROFF_CODES_REBOOT_HOSTMODE,
		CR2_POWEROFF_CODES_REBOOT_MAX,
	} CR2_poweroff_reason_codes_t;

	typedef struct _CR_arg_t {
		I32		len;
		U32 	in_out;
		U64		arg;
		U32 	reserved[4];
	} CR_arg_t;

	typedef		enum _CR_arg_type_t {
		CR_ARG_TYPE_IN = 0x0,
		CR_ARG_TYPE_OUT,
		CR_ARG_TYPE_RET,
	} CR_arg_type_t;

	typedef 	enum _CR_func_call_type_t {
		CR_FUNC_NONE = 0x0,
		CR_CURL_GENERIC_GETCOOKIE,
		CR_CURL_GENERIC_UPLOADFILE,
	} CR_func_call_type_t;

	enum {
		READYBALL_STATUS_eNULL,
		READYBALL_STATUS_eNOBALL,
		READYBALL_STATUS_eREADY,
		READYBALL_STATUS_eOUTOFZONE,
	};


	///////////////////////////////////////////////////
#ifndef _APP_SECURE_H
#define DEVICE_TYPE_EYEMINI 		5010
#define DEVICE_TYPE_EYEMINI_STR		"5010"
#define DEVICE_TYPE_EMLITE_DRAFT	4210
#define DEVICE_TYPE_EMLITE_DRAFT_STR "4210"
#define DEVICE_TYPE_EMLITE			4211
#define DEVICE_TYPE_EMLITE_STR		"4211"
#endif

#define CCODE_LEN				16		// 16*(sizeof u32)

///////////// Secure Data /////////////////////////
#define DEV_SN_SIZE				16		// max 15 char + '\0'
#define DEV_TYPE_STR_SIZE		16		// max 15 char + '\0'
#define REGION_CODE_STR_SIZE	16		// max 15 char + '\0'
#define CLIENT_CODE_SIZE		32
#define LICENSE_CODE_SIZE		32
#define LICENSE_STR_SIZE		16		// 15 char + '\0'
#define ACT_CODE_SIZE			32
#define USER_ID_SIZE			64		// max 63 char + '\0'
#define ACCOUNT_STR_SIZE		(16+1)	// 16 char + '\0'
#define MAX_ENC_PAYLOAD_SIZE	512

#define SECURE_ACK_SENDER_CRC_ERR (-1)
#define SECURE_ACK_SENDER_PARAM_ERR (-2)
#define SECURE_ACK_SENDER_ENCRYPT_ERR (-3)
#define SECURE_ACK_SENDER_INVALID (-4)
#define SECURE_ACK_DEVICE_ERR (0)
#define SECURE_ACK_SUCCESS (1)
#define SECURE_ACK_DEVICE_PAIR_ALREADY (2)
#define SECURE_ACK_DEVICE_PAIR_FULL (3)

	typedef struct _encryptedData {
		U32 payload_length;
		U08 enc_payload[MAX_ENC_PAYLOAD_SIZE];
		U32 crc;
	} EncryptedData_T;

	typedef struct _ProductInfo {
		U08		device_serial_number[DEV_SN_SIZE];			// 16 bytes
		char	device_type_string[DEV_TYPE_STR_SIZE];		// 16 characters
		U64		region_code;								// 8 bytes
		char	region_code_string[REGION_CODE_STR_SIZE];	// 16 bytes
		U32		device_type;								// 4 bytes
		U32		crc;										// 4 bytes
	} ProductInfo_T;	// Total 64 Bytes


	typedef struct _ProductSecret {
		I32 success;
		U32 device_unique_id[8];	// 32 byte
		U08 compk1[16];				// 16 byte, AES plain key
		U08 reserved[8];			// padding for data alignment
		U32 crc;					//  4 byte, 1 int
	} ProductSecret_T;	// Total 64 Bytes

	typedef struct _GetUpdate {
		U32 device_unique_id[8];
		U08 reserved[12];
		U32 crc;
	} GetUpdate_T;	// Total 48 Bytes

	typedef struct _GetUpdatedData {
		U64 utc_time_tick;
		U64 cur_region_code;
		U32 renewal_num;
		U08 reserved[8];
		U32 crc;
	} GetUpdatedData_T;	// Total 32 Bytes


	typedef struct _LicenseInfo {
		U64		expiry_time;
		char	user_id[USER_ID_SIZE];
		U64		region_code;
		char	region_code_string[REGION_CODE_STR_SIZE];
		char	license_string[LICENSE_STR_SIZE];
		U08		license_code[LICENSE_CODE_SIZE];
		U08		act_code[ACT_CODE_SIZE];
		U32		renewal_num;
		U32		pairing_renewal_limit;
		U32		offline_max_min;
		U08		license_type;
		char	account_string[ACCOUNT_STR_SIZE];
		U08		reserved[46];	// padding for data alignment
		U32		crc;
	} LicenseInfo_T; /* total 256bytes */



	typedef struct _ClientInfo {
		U08		client_code[CLIENT_CODE_SIZE];		// 32 byte
		char	account_string[ACCOUNT_STR_SIZE];
		U08		client_type;						// pc
		U08		reserved[10];						// padding for data alignment
		U32		crc;
	} ClientInfo_T;	// Total 64 Bytes

	typedef struct _ClientInfoStored {
		U08		client_code[CLIENT_CODE_SIZE];		// 32 byte
		char	account_string[ACCOUNT_STR_SIZE];
		U08		client_type;						// pc
		U08		reserved[6];						// padding for data alignment
		U32		pairing_renewal;
		U32		crc;
	} ClientInfoStored_T;	// Total 64 Bytes

	typedef struct _PairingResult {
		I32 success;
		U32 pairing_renewal;
		U08 reserved[4];	// padding for data alignment
		U32 crc;
	} PairingResult_T;	// Total 16 Bytes

	typedef struct _UnpairingRequest {
		U08 client_type;
		U08 reserved[11];	// padding for data alignment
		U32 crc;
	} UnpairingRequest_T;	// Total 16 Bytes

	typedef struct _SecureCmdResult_T {
		I32 success;
		U08 reserved[8];	// padding for data alignment
		U32 crc;
	} SecureCmdResult_T;	// Total 16 Bytes

#define UnpairingResult_T SecureCmdResult_T

	typedef struct _GetActCode {
		I32 success;
		U08 act_code[ACT_CODE_SIZE];
		U08 reserved[8];
		U32 crc;
	} GetActCode_T;	// Total 48 Bytes

	/* PC launcher license interface */
#define MAX_PCL_LICENSEDATA_SIZE	1008
	typedef struct _LicensePayload_PCL {
		char encrypted_data[MAX_PCL_LICENSEDATA_SIZE];
		U32 size; /* real data size */
		U08 reserved[8];
		U32 crc;
	} LicensePayload_PCL_T;	// Total 1024 bytes

	typedef struct _LicenseCheckAck_PCL {
		I32 success;
		U32 need_license_renewal;	/* if 1, license update is needed */
		U08 reserved[4];
		U32 crc;
	} LicenseCheckAck_PCL_T;	// Total 16 Bytes


	typedef struct _LicenseAck_PCL {
		I32 success;
		U32 license_renewal_num;	 /* latest renewal count in device */
		U08 reserved[4];
		U32 crc;
	} LicenseAck_PCL_T;	// Total 16 Bytes

#define MAX_PCL_SNDATA_SIZE	256
	typedef struct _SnPayload_PCL {
		char encrypted_data[MAX_PCL_SNDATA_SIZE];
		U32 size; /* real data size */
		U08 reserved[8];
		U32 crc;
	} SnPayload_PCL_T;	// Total 272 bytes


	///////////////////////////////////////////////////
	// redefine structure
	///////////////////////////////////////////////////
#ifndef _CR_COMMON_H_
	typedef struct _CR_Version_t {
		unsigned int 	ver_major;
		unsigned int 	ver_minor;
		unsigned int 	build_num;
	} CR_Version_t;

	typedef struct {
		char 			version_str[63 + 1];
		CR_Version_t	version;
		char			url[511 + 1];
		char			checksum[256 + 1];
	} CR_UpgradeFile_t;
#endif

#ifndef _APP_LOADER_H_
#define AVAILABLE_SOFTWARE_MAX_LIST		5
	typedef struct {
		U32					stage;
		U32					target;
		U32					forced;

		CR_Version_t 		main_version;
		CR_Version_t 		fpga_version;
		CR_Version_t 		system_version;

		CR_UpgradeFile_t	main_sw;
		CR_UpgradeFile_t	unified_sw;
	} appUpgradeFile_t;
#endif

#ifndef _WIFI_DRV_H_
	typedef struct _wifi_info_t
	{
		I08		type;		//wifiType_t
		I08		status;		//wifiStatusType_t
		U08		mode;				//wifiMode_t
		char	ipAddr[128];
		char	ssid[32];
		char	password[256];
		char	quality[32];
		char	level[32];
		char	encryption[4];
	} wifi_info_t;

	typedef struct _wifi_list_t
	{
		I32 number;						// (-1) no wifi
		wifi_info_t lists[100];
	} wifi_list_t;

	typedef enum
	{
		WIFI_STATUS_DEFAULT,
		WIFI_STATUS_CONNECTED,
		WIFI_STATUS_MAX
	} wifiStatusType_t;

	typedef enum
	{
		WIFI_TYPE_DEFAULT,
		WIFI_TYPE_NORMAL,
		WIFI_TYPE_HIDDEN,
		WIFI_TYPE_RECONNECT,
		WIFI_TYPE_MAX
	} wifiType_t;
#endif

#ifndef _CR_OSAPI_H_
	typedef struct _CR_Time_t {
		I32 wYear;
		I32 wMonth;
		I32 wDayOfWeek;
		I32 wDay;
		I32 wHour;
		I32 wMinute;
		I32 wSecond;
		I32 wMilliseconds;
	} CR_Time_t;
#endif

#ifndef _CR_SERVICEAPI_H_
	typedef struct {
		char	device_uid_str[64 + 1];
		char	data[1024 + 1];
	} SvcReqEncDataPayload_t;

	typedef struct {
		char	data[2048];
	} SvcResEncDataPayload_t;
#endif

#ifndef _GPS_DRV_H_
	typedef struct GPS_Pos_ {
		double minutes;
		int degrees;
		char cardinal;
	} GPS_Pos_T;
#endif
	///////////////////////////////////////////////////

	typedef struct {
		U32 model_id;
		char model_name[32];

		char hw_version_str[32];
		U32 hw_version;

		char sw_version_str[32];
		CR_Version_t sw_version;

		char fpga_version_str[64];
		CR_Version_t fpga_version;
		char fpga_pos[16];
		char fpga_build_date[32];

		char system_version_str[32];
		CR_Version_t system_version;

		char serial_number[DEV_SN_SIZE];
		U32 device_uid[8];
		char device_uid_str[32 * 2 + 1];

		U64 region_code;

		char hw_revision_str[32];
		U32 hw_revision;

		U08 reserved[32];
	} appSyscfgParameter_t;

	//---- Call back fucntion for shot event
#if defined(_WIN64)
	typedef I32(CALLBACK CR2_CALLBACKFUNC) (HAND h, U32 status, CR2_shotdata_t* psd, I64 userparam);
#else
	typedef I32(CALLBACK CR2_CALLBACKFUNC) (HAND h, U32 status, CR2_shotdata_t* psd, I32 userparam);
#endif

#if defined(_WIN64)
	typedef I32(CALLBACK CR2_CALLBACKFUNC1) (HAND h, U32 status, HAND hsd, U32 cbfuncid, I64 userparam);
#else
	typedef I32(CALLBACK CR2_CALLBACKFUNC1) (HAND h, U32 status, HAND hsd, U32 cbfuncid, I32 userparam);
#endif

#if defined(_WIN64)
	typedef I32(CALLBACK CR2_CALLBACKFUNC2) (HAND h, U32 command, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3, U32 callback_count, I64 userparam);
#else
	typedef I32(CALLBACK CR2_CALLBACKFUNC2) (HAND h, U32 command, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3, U32 callback_count, I32 userparam);
#endif

	//---- Call back fucntion for Image capture
	typedef I32(CALLBACK CR2_CAM_CALLBACKFUNC) (HAND h, I32 chnum, U08* pimg, I32 len);

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
	CR_CR2ADAPT_EXPORT char* CR2_GetInstallDir(void);


#if defined (__cplusplus)
}
#endif


#endif		// _CR_CR2ADAPT_