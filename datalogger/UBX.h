const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };

struct NAV_PVT {
    unsigned char   cls;
    unsigned char   id;
    unsigned short  len;
    unsigned long   iTOW; //GPS Time of week
    uint16_t        year;
    uint8_t         month;
    uint8_t         day;
    uint8_t         hour;
    uint8_t         min;
    uint8_t         sec;

    unsigned char   valid; 
    //valid Bitflags: 
    //4 = fullyResolved, 
    //2 = validDate, 
    //1 = validTime

    unsigned long   tAcc; //Time accuracy extimate (UTC)
    long            nano; //fraction of second range.
    uint8_t         fixType;
    //fixType values:
    //0: no fix
    //1: dead reckoning only
    //2: 2D-fix
    //3: 3D-fix
    //4: GNSS + dead reckoning
    //5: Time only

    unsigned char   flags; //fix status flags
    unsigned char   flags2; //additional flags
    uint8_t         numSV; //Number of satellites
    long            lon; //Longitude. Scaling: 1e-7
    long            lat; //Latitude. Scaling: 1e-7
    long            height; //Height above ellipsoid
    long            alt; //Height above mean sea level
    unsigned long   hAcc; //Horizontal accuracy estimate
    unsigned long   vAcc; //Vertical accuracy estimate
    long            velN; //NED North Velocity
    long            velE; //NED East Velocity
    long            velD; //NED Down Velocity
    long            gSpeed; //Ground speed (2-D)
    long            headMot; //Heading of motion
    unsigned long   sAcc; //Speed accuracy estimate
    unsigned long   headAcc; //Heading accuracy estimate
    uint16_t        pDOP; //Position DOP
    uint8_t         reserved1[6]; //Reserved
    long            headVeh; //heading of vehicle (2-D)
    uint8_t         reserved2[4]; //Reserved
};

struct NAV_VELNED {
    unsigned char   cls;
    unsigned char   id;
    unsigned short  len;
    unsigned long   iTOW;
    long            velN;
    long            velE;
    long            velD;
    unsigned long   spd;
    unsigned long   gSpd;
    long            hdg;
    unsigned long   sAcc;
    unsigned long   hAcc;
};