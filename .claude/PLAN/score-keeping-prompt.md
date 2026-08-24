Create a score-keeping app baed on PayloadCMS +  NesxtJS
The PWA-App will become a route under that application.

The app going to get a considerable expansion based on PayloadCMS + NextJS
The current PWA-app will become a route in NextJS as part of a bigger application.

The application must track a shooters score during a match. Each match has several competing squads (a group of people competing in the same , each squad consists of several competers.
A shooter has a card with a barcode representing their KNSA Membership number
A barcode scanner will be used to track the active shooter.

The barcode scanner is a 2D/3D HID scanner that is reconize as a USB keyboard. (NETUM NT-EM61 CMOS scanner)


A timekeepers screen is needed (under /admin, a login is requried).

This screen displays the current shooter's name and time. Split times are not displayed here.
It also displays the squad list. The current active shooters name is highlighted.
The names are clickable allowing the timekeeper to manually set the current active shooter, incase the barcode scanner does not
work.
The can only be done when there is no active session. Acidentily this also the only time
the barcode scanner is active.


It has a button to advance the match to the next round, allowing the next shooter to scan their card


We need a way to enter shooters in the system.
Atleast a first name, lastname, ASN number, KNSA number.


## Definitions
### Competer / shooter

### Time keeper
Sits behind the timekeeping screen.
Advances the match to the next shooter.


### Range Office
Has the timer and presses start/stop button on timer device.


#### Squad
A group of people currently competeting on the range.
A single squad can have multiple disciplines going on.

Basically the sqaud is used to determine which people are on the range at a time block.

A squad has time schedule (08:00 to 09:00)
a list of shooters
```
| 08:00 to 09:00 |
|#<number> | <name> | <discipline> | <ASN Membership number> | <KNSA Membership number> |
|#1 | John Wick | OKP | <ASN Membership number> | 211764 |
|#2 | Peter Piper | SKP | <ASN Membership number> | 1111111 |
``

Expample squad schedule here:
file:///C:/Users/Jorgen/Downloads/squad_schedule.pdf

Please note shooters compete in multple disciplines
Shooters can be in multiple squads


# Matchs disciplines:
OKP: Open groot kaliber pistool
OKKP:open klein kaliber pistool
SKP: Standaard groot kaliber pistool
SKKP:Standaard klein kaliber pistool
PCC 9mm
PCC .22
OKR: Open groot kaliber revolver
OKKR: Open klein kaliber revolver
SKR: Standard Revolver
SKKR: Standaard klein kaliber Revolver

Defined as an ENUM in code



## How matches are run:

1. Active squad is on the range
2. (Next) shooter of the squad scans their barcode to become the active shooter
3. Range office presses start button on timer device
4. Shooter shoots their round.
5. Range office presses stop button on timer device. Round ends.
6. Time- keeper advances to the next shooter (for a total of 5 rounds). Goto 2.
6.1 After last round, shooter confirms recorded times with timekeeper.*
7. When all squadmembers have finished 5 rounds that macht is over for the active squad
8. New sqaud arives on the range, GOTO 1.

A shooter is allowed one reshoot if he/she has a malfunction during on of the match rounds.


*Note for 6.1
- We do this on paper and have the shooter sign off with autograph.
- When the squad is finished, the timekeeper brings the collected papers to match director
- match redirector then puts it in a external system for end results after the match.

