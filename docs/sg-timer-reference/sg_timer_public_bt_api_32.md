# SMART SHOT TIMER

BLE API VERSION 3.2

The device advertises 128-bit UUID of main Service – 7520FFFF-14D2-4CDA-8B6B-697C554C9311 and name – SG-SST4XYYYYY

where **X** is a model identifier:
	‘A’ for SG Timer Sport
	‘B’ for SG Timer GO

**YYYYY** – is the device serial number




## Table 1 – BLE Attribute Table

|Name |Type |UUID |Properties
------------
[MAIN](#1.MAIN) |Service|7520FFFF-14D2-4CDA-8B6B-697C554C9311 –
[COMMAND](#1.1COMMAND) |Characteristic|75200000-14D2-4CDA-8B6B-697C554C9311 | W, N
[EVENT](#1.2EVENT) |Characteristic|75200001-14D2-4CDA-8B6B-697C554C9311 |N
[SAVED_SESSION_ID_LIST](#1.3SAVED_SESSION_ID_LIST) | Characteristic |75200002-14D2-4CDA-8B6B-697C554C9311 | R,W
[RESERVED](#1.4RESERVED) |Characteristic|75200003-14D2-4CDA-8B6B-697C554C9311 |R
[SHOT_LIST](#1.5SHOT_LIST) |Characteristic|75200004-14D2-4CDA-8B6B-697C554C9311 | R, W
[PAR_SETUP](#1.6PAR_SETUP) |Characteristic|75200005-14D2-4CDA-8B6B-697C554C9311 | R, |W
[UNIX_TIME](#1.7UNIX_TIME) |Characteristic|75200006-14D2-4CDA-8B6B-697C554C9311 | R, W
[API_VERSION](#1.8API_VERSION) |Characteristic|7520FFFE-14D2-4CDA-8B6B-697C554C9311 |R

Notes:

R – Read
W – Write
N – Notify
I – Indicate
All multibyte values in any characteristic are represented in Big Endian format


## <a name="1.MAIN"></a>1. MAIN

Main Service of the application
Back to attribute table

### <a name="1.1COMMAND"></a>1.1 COMMAND

Characteristic is used to execute commands
The general format of any command is given below
Field size 11n
Field name lencmd_idcmd_data
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
cmd_data– command data. See Command Table
After command is received the response is sent by characteristic notification
The general format of any command response is given below
Field size 111
Field name lencmd_idresp_code
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
resp_code– code of the response. See Response Codes
Commands can be send c onsecutively one by one without waiting for a response to
every command. Responses (as well as commands) can be easily parsed in the byte
stream due to the packet size in the first byte.
Table 2 – Command Table
Command Name Command ID
SESSION_START 0x00
SESSION_SUSPEND 0x01
SESSION_RESUME 0x02
SESSION_STOP 0x03
Response Codes:
0x00 – Success
0x01 – Error
Back to attribute table

#### 1.1.1 SESSION_START

Command is used to start RO session
Field size11
Field namelencmd_id
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
Back to attribute table

#### 1.1.2 SESSION_SUSPEND

Command is used to suspend the current RO session
Field size11
Field namelencmd_id
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
Back to attribute table

#### 1.1.3 SESSION_RESUME

Command is used to resume the current RO session
Field size11
Field namelencmd_id
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
Back to attribute table

#### 1.1.4 SESSION_STOP

Command is used to stop the current RO session
Field size11
Field namelencmd_id
len– number of bytes following the current byte
cmd_id – id of the command. See Command Table
Back to attribute table

## <a name="1.2EVENT"></a>1.2 EVENT

The characteristic is used to notifying of events that occur with the device.
The supported events are summarized in the table below
Table 3 – Events Table
Event Name Event ID
SESSION_STARTED 0x00
SESSION_SUSPENDED 0x01
SESSION_RESUMED 0x02
SESSION_STOPPED 0x03
SHOT_DETECTED 0x04
SESSION_SET_BEGIN 0x05
Back to attribute table

### 1.2.1 SESSION_STARTED

Event is sent by timer when RO session has been started
Field size1142
Field namelenevent_idsess_idstart_delay
len– number of bytes following the current byte
event_id – id of the event. See Events Table
sess_id– id of started session (unix time stamp)
start_delay– start delay of started session in units of 0.1 second
Back to attribute table

### 1.2.2 SESSION_SUSPENDED

Event is sent by timer when RO session has been suspended
Field
size1142
Field
namelenevent_idsess_idtotal_shots
len– number of bytes following the current byte
sess_id – id of the session (unix time stamp)
total_shots – number of shots detected
event_id – id of the event. See Events Table
Back to attribute table

### 1.2.3 SESSION_RESUMED

Event is sent by timer when RO session has been resumed
Field
size1142
Field
namelenevent_idsess_idtotal_shots
len– number of bytes following the current byte
sess_id – id of the session (unix time stamp)
total_shots – number of shots detected
event_id – id of the event. See Events Table
Back to attribute table

### 1.2.4 SESSION_STOPPED

Event is sent by timer when RO session has been stopped
Field
size1142
Field
namelenevent_idsess_idtotal_shots
len– number of bytes following the current byte
sess_id – id of the session (unix time stamp)
total_shots – number of shots detected
event_id – id of the event. See Events Table
Back to attribute table

### 1.2.5 SHOT_DETECTED

Event is sent by timer when shot has been detected (RO session only)
Field size11424
Field
namelenevent_idsess_idshot_numshot_time
len– number of bytes following the current byte
event_id – id of the event. See Events Table
sess_id– id of the session (unix time stamp)
shot_num– number of detected shot
shot_time– shot time in units of 1 millisecond
Back to attribute table

### 1.2.6 SESSION_SET_BEGIN

Event is sent by timer when delay time ends and session set starts
Field size114
Field
namelenevent_idsess_id
len– number of bytes following the current byte
event_id – id of the event. See Events Table
sess_id– id of the session (unix time stamp)
Back to attribute table

## <a name="1.3SAVED_SESSION_ID_LIST"></a>1.3 SAVED_SESSION_ID_LIST

Characteristic is used to read saved session ids.
Characteristic write format is as follows
Field size4
Field namesess_id
sess_id– id of the session from which the session list will be started  (unix time
stamp). When this field is 0xFFFFFFFF then the last (newest) session id will
be read
Characteristic read format is as follows
Field size4
Field namesess_id
sess_id– id of the session (unix time stamp)
Repeated readings from this characteristic must be performed to read all available saved
session ids. Next reading after the last (oldest) session will give the 0xFFFFFFFF value
which points to the end of the list. Next reading after this will give the first (oldest)
session id (wraparound will occur).
Session ids reads in reverse order (from newest to oldest).
Back to attribute table

## <a name="1.4RESERVED"></a>1.4 RESERVED

Characteristic is reserved for future use
Back to attribute table

## <a name="1.5SHOT_LIST"></a>1.5 SHOT_LIST

Characteristic is used to read the shots of the particular session.
Characteristic write format is as follows
Field size4
Field namesess_id
sess_id– id of the session from which the shots will be read (unix time stamp)
Characteristic read format is as follows
Field size 24
Field nameshot_numbershot_time
shot_number– shot number (starts from 0)
shot_time– shot time in units of 1 millisecond
Repeated readings from this characteristic must be performed to read all available shots.
Next reading after the last shot will give the 0xFFFFFFFF value in shot_time which
points to the end of the list. Next reading after this will give the first shot (wraparound will
occur).
Writing the session id to this characteristic resets the shot number to zero, so that the
first read after write will always give the first shot.
Back to attribute table

## <a name="1.6PAR_SETUP"></a>1.6 PAR_SETUP

Characteristic is used to read and write PAR configuration of the RO session.
Characteristic read/write format is as follows
Field size222
Field
namestart_delaytime_limitshot_limit
start_delay– start delay of started session in units of 0.1 second. If this value is
0xFFFF then random delay in range 1.0 – 4.0 seconds will be used
time_limit– session time limit in units of 0.1 second. If this value is 0  then time
will be unlimited
shot_time– session shot limit. If this value is 0  then shots will be unlimited
Back to attribute table

## <a name="1.7UNIX_TIME"></a>1.7 UNIX_TIME

Characteristic is used to read or write the device local time.
Characteristic read/write format is as follows
Field size4
Field nameunix_time
unix_time– local time value in units of seconds that have elapsed since Unix Epoch
Back to attribute table

## <a name="1.8API_VERSION"></a>1.8 API_VERSION

Characteristic is used to read the current version of API implemented into the timer firmware.
Format is the non-terminated ASCII string. For example version 1.0 will read as:
HEX 0x31 0x3E 0x30
ASCII '1' '.' '0'
Back to attribute table