# ESP32 KIM-1
ESP32 + FabGL based KIM-1 serial mode emulator
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/HUjK1wHHkJg/0.jpg)](https://youtu.be/HUjK1wHHkJg)

# Memory layout
    0x0000 - 0x16FF    5888 bytes RAM
    0x1700 - 0x17FF     256 bytes RAM 6530 RIOT chips (I/O, Timers)
    0x1800 - 0x1FFF    2048 bytes ROM 6530 RIOT chips (KIM operating system)
    0x2000 - 0x28FF    2304 bytes RAM Tiny Basic by Tom Pittman
    0x9548 - 0xA032    2794 bytes ROM Supermon by Jim Butterfield
    0xFFFA - 0xFFFF       6 bytes ROM IRQ table

# User manual
    Select an Address

    Type four hex keys (0 to F) to define the desired address.
    Next, press the [SPACE] bar.
    The printer will respond showing the address code selected
    followed by a two digit hex code for data stored at the selected
    address location:
       Type:                 1234        [SPACE]
       Printer Responds:     1234        AF
    showing that the data AF is stored at location 1234.

    Modify Data

    Select an address as in the previous section.  Now type two hex
    characters to define the data to be stored at that address.  Next type
    the [.] key to authorize the modification of data at the selected address:
       Type:                  1234  [SPACE]
       Printer Responds:      1234  AF
       Type:                              6D    [.]
       Printer Responds:      1235  B7
    Note that the selected address (1234) has been modified and the system
    increments automatically to the next address (1235).

    Note:  Leading zero's need not be entered for either address
         or data fields:  For example:
              EF [SPACE] selects address 00EF
               E [SPACE] selects address 000E
               A [.] enters data 0A
                 [.] enters data 00 (etc.)

    Step to Next Address

    Type [CR] to step to the next address without modifying the
    current address:
       See Printed:            1234  AF
       Type:                             [CR]
       Printer Responds:       1235  B7
       Type:                             [CR]
       Printer Responds:       1236  C8          (etc.)

    Load Paper Tape

    Paper Tapes suitable for use with the KIM-1 system are generated
    using the format shown in Appendix F.  To read such a tape into the KIM-1
    system, proceed as follows:

       1. Type [L]
       2. Run command: 'sudo python3 ./path/to/file.ptp'

    The paper tape will advance and data will be loaded into addresses
    as specified on the tape.  A printed copy of the data read will be generated
    simultaneously with the reading of the paper tape.

    Check-sums are generated during the reading of the paper tape
    and are compared to check-sums already contained on the tape.  A check-
    sum error will cause an error message to appear in the printed copy.


    Punch Paper Tape

    The KIM-1 system can be used to punch paper tapes having the
    format described in Appendix F.  The procedures for generating these
    tapes is as follows:

       1. Define the starting address and ending address of the
          data block to be punched on the paper tape.

       2. Load blank paper tape on the punch unit and activate
          the punch.

             Type:                       [1] [7] [F] [7] [SPACE]
             See Printed:    17F7    xx
             Type:                       [F] [F] [.]
             See Printed:    17F8    xx
             Type:                       [0] [3] [.]
             See Printed:    17F9    xx
             Type:                       [2] [0] [0]     [SPACE]
             See Printed:    0200    xx

    You have now loaded the ending address (03FF) into address
    locations 17F7 (EAL) and 17F8 (EAH).  The starting address (0200) is
    selected as shown.

       3. Now type [Q]

          The paper tape will advance and punching of the data
          will proceed.  Simultaneously, a printed record of
          the data will be typed.


    List Program

    A printed record of the contents of the KIM-1 memory may be
    typed.  The procedure is the same as for punching paper tape except that
    the punch mechanism is not activated.


    Execute Program

    To initiate execution of a program using the TTY keyboard, the
    following procedures should be followed:

       1. Enter the starting address of the program

       2. Type [G]
          For example, to begin program execution from
          address location 0200:
               Type:           [2] [0] [0] [SPACE]
               See Printed:    0200    xx
               Type:           [G]

          Program execution begins from location 0200 and will
          continue until the [ST] or [RS] keys of the KIM-1
          module are depressed.  The single step feature may
          be employed while in the TTY mode.

# Run Supermon+64
 - Type in: 9548 [Space]
 - Type in: [G]

# Run BASIC
 - Type in: 2000 [Space] (Cold start)
 - Type in: 2003 [Space] (Warm start, after LOAD/SAVE)
 - Type in: [G]







