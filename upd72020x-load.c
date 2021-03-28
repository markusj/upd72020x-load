#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define FAILED " ======> FAILED\n"
#define PASSED " ======> PASSED\n"
#define LOOPNB 100000
#define POLL_US 10
#define DELAY_US 1000
#define ROM_PARAM_INVALID 0xffffffff

#define EXT_FW_VERSION 0x6C
#define EXT_ROM_INFO_REG 0xEC
#define EXT_ROM_CONFIG_REG 0xF0
#define EXT_FW_DLOAD_CTRL_STATUS 0xF4
#define EXT_ROM_CTRL_STATUS 0xF6
#define EXT_DATA0 0xF8
#define EXT_DATA1 0xFC

// FW Control and Status Register
#define ROM_ACCESS_ENABLE 0
#define ROM_ERASE 1
#define ROM_RELOAD 2
#define ROM_RESULT_0 4
#define ROM_RESULT_1 5
#define ROM_RESULT_2 6
#define ROM_SET_DATA0 8
#define ROM_SET_DATA1 9
#define ROM_GET_DATA0 10
#define ROM_GET_DATA1 11
#define ROM_EXISTS 15

// FW Download Control and Status Register
#define FW_DLOAD_ENABLE 0
#define FW_DLOAD_LOCK 1
#define FW_RESULT_0 4
#define FW_RESULT_1 5
#define FW_RESULT_2 6
#define FW_SET_DATA0 8
#define FW_SET_DATA1 9

#define RESULT_BITMASK (0x0070)
#define RESULT_INVALID (0x0000)
#define RESULT_SUCCESS (0x0010)
#define RESULT_ERROR   (0x0020))

#define DATAREG(index) \
    (((index & 0x1) != 0) ? EXT_DATA1 : EXT_DATA0)

#define RETURN_ON_ERR(condition, msg, ...) \
    if (condition) { \
        printf(msg, ##__VA_ARGS__); \
        return -1; \
    }

u_int lookup_rompar(const u_int rominfo) {
    switch (rominfo) {
        case 0x00C22010: // MX25L512E
        case 0x00C22011: // MX25L1006E
        case 0x00C22012: // MX25L2006E
        case 0x00C22013: // MX25L4006E
            return 0x700;
        case 0x00C22210: // MX25L5121E
        case 0x00C22211: // MX25L1021E
            return 0x500;
        case 0x00EF3011: // W25X10BV
        case 0x00EF3012: // W25X20BV
        case 0x00EF3013: // W25X40BV
            return 0x700;
        case 0x00202010: // M25P05-A
        case 0x00202011: // M25P10-A
            return 0x750;
        case 0x00202012: // M25P20
        case 0x00202013: // M25P40
            return 0x760;
        case 0x005e2013: // T25S40, undocumented but working in 0x700 mode
        case 0x019D20FF: // Pm25LD512C
        case 0x019D207F: // Pm25LD512C2
        case 0x001F6500: // AT25F512B
        case 0x001C3110: // EN25F05
        case 0x001C3111: // EN25F10
        case 0x001C3112: // EN25F20
        case 0x001C3113: // EN25F40
        case 0x00373010: // A25L512
        case 0x00373011: // A25L010
        case 0x00373012: // A25L020
        case 0x00373013: // A25L040
            return 0x700;
        case 0x00BF0048: // SST25VF512A
        case 0x00BF0049: // SST25VF010A
            return 0x10791;
        default:
            return ROM_PARAM_INVALID;
    }
}

int pci_cfg_read16(int fd, u_int off, u_int * val16) {
    lseek(fd, off, SEEK_SET);

    return read(fd, val16, 2);
}

int pci_cfg_write16(int fd, u_int off, u_int val16) {
    lseek(fd, off, SEEK_SET);

    return write(fd, &val16, 2);
}

int pci_cfg_read32(int fd, u_int off, u_int * val32) {
    lseek(fd, off, SEEK_SET);

    return read(fd, val32, 4);
}

int pci_cfg_write32(int fd, u_int off, u_int val32) {
    lseek(fd, off, SEEK_SET);

    return write(fd, &val32, 4);
}

/**
 * Support function for reading certain bits from a register
 *
 * @param fd
 *      card file descriptor
 * @param reg
 *      register address
 * @param bitmask
 *      bit mask used to extract the desired bits
 * @return
 *      extracted bits
 */
int read_bitmask(u_int fd, u_int reg, u_int bitmask, u_int * value) {
    int result;

    result = pci_cfg_read16(fd, reg, value);

    *value &= bitmask;

    return result;
}

/**
 * Support function for writing certain bits to a register
 *
 * @param fd
 *      card file descriptor
 * @param reg
 *      register address
 * @param bitmask
 *      bitmask used to select which bits to write
 * @param value
 *      value from which the masked bits should be written into the register
 * @return
 *      error code
 */
int write_bitmask(u_int fd, u_int reg, u_int bitmask, u_int value) {
    int result;
    u_int val;

    result = pci_cfg_read16(fd, reg, &val);

    if (result >= 0) {
        val &= ~bitmask;
        val |= (value & bitmask);

        return pci_cfg_write16(fd, reg, val);
    } else {
        return result;
    }
}

/**
 * Support function for reading a bit from a register
 *
 * @param fd
 *      card file descriptor
 * @param reg
 *      register address
 * @param bit
 *      bit offset
 * @return
 *      bit value
 */
int read_bit(u_int fd, u_int reg, u_int bit, u_int * value) {
    return read_bitmask(fd, reg, 1 << bit, value);
}

/**
 * Support function for writing a bit to a register
 *
 * @param fd
 *      card file descriptor
 * @param reg
 *      register address
 * @param bit
 *      bit offset
 * @param value
 *      bit value
 * @return
 *      error code
 */
int write_bit(u_int fd, u_int reg, u_int bit, u_int value) {
    return write_bitmask(fd, reg, 1 << bit, value << bit);
}

int eeprom_exists(int fd) {
    u_int reg;

    // is ROM present?
    RETURN_ON_ERR(
        read_bit(fd, EXT_ROM_CTRL_STATUS, ROM_EXISTS, &reg) < 0,
        "ERROR: PCI CFG read of EXT_ROM_CTRL_STATUS register failed\n"
    );

    return reg == 0 ? -1 : 0;
}

int external_rom_access(int fd, bool enable) {
    u_int reg, ix;

    RETURN_ON_ERR(eeprom_exists(fd) < 0, "ERROR: ROM doesnt exist\n");

    if (enable) {
        RETURN_ON_ERR(
            pci_cfg_write32(fd, EXT_DATA0, 0x53524F4D) < 0,
            "ERROR: PCI CFG write of EXT_ROM_DATA0 register failed\n"
        );

        usleep(DELAY_US);

        RETURN_ON_ERR(
            write_bit(fd, EXT_ROM_CTRL_STATUS, ROM_ACCESS_ENABLE, 1) < 0,
            "ERROR: PCI CFG write to enable ROM access failed\n"
        );

        for (ix = 0; ix < LOOPNB; ix++) {
            usleep(POLL_US);

            pci_cfg_read16(fd, EXT_ROM_CTRL_STATUS, &reg);

            if ((reg & RESULT_BITMASK) == RESULT_INVALID) {
                return 0;
            }
        }

        printf("cant enable ext rom access\n");

        return -1;
    } else {
        RETURN_ON_ERR(
            write_bit(fd, EXT_ROM_CTRL_STATUS, ROM_ACCESS_ENABLE, 0) < 0,
            "ERROR: PCI CFG write to disable ROM access failed\n"
        );

        return 0;
    }
}

int read_eeprom(int fd, char *filename, unsigned int len) {
    int ofile;
    u_int ix, data01, jx, val32, status;

    ofile = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0644);

    RETURN_ON_ERR(ofile < 0, "ERROR: cant open file %s\n", filename);

    // enable access
    RETURN_ON_ERR(
        external_rom_access(fd, true) < 0,
        "ERROR: cant enable access to ROM \n"
    );

    sleep(2);

    RETURN_ON_ERR(
        write_bit(fd, EXT_ROM_CTRL_STATUS, ROM_GET_DATA0, 1) < 0,
        "ERROR: cant set GET_DATA0\n"
    );
    RETURN_ON_ERR(
        write_bit(fd, EXT_ROM_CTRL_STATUS, ROM_GET_DATA1, 1) < 0,
        "ERROR: cant set GET_DATA1\n"
    );

    sleep(2);

    for (ix = 0; ix < len / 8; ix++) {
        for (data01 = 0; data01 < 2; data01++) {
            u_int databit = ROM_GET_DATA0 + data01;

            for (jx = 0; jx < LOOPNB; jx++) {
                usleep(POLL_US);

                if ((read_bit(fd, EXT_ROM_CTRL_STATUS, databit, &status) >= 0)
                        && (status == 0)) {
                    break;
                }
            }

            RETURN_ON_ERR(jx == LOOPNB, "ERROR: GET_DATAx never go to zero\n");

            usleep(POLL_US);

            //read eeprom
            RETURN_ON_ERR(
                pci_cfg_read32(fd, DATAREG(databit), &val32) < 0,
                "ERROR: PCI CFG read of EXT_ROM_DATAx register failed\n"
            );

            write(ofile, &val32, 4);

            RETURN_ON_ERR(
                write_bit(fd, EXT_ROM_CTRL_STATUS, databit, 1) < 0,
                "ERROR: cant set GET_DATAx\n"
            );

        }
    }

    RETURN_ON_ERR(
        external_rom_access(fd, false) < 0,
        "ERROR: cant DISABLE access to ROM\n"
    );

    return 0; //Success!

}

int do_upload(int fd, int ifile, u_int ctrl_reg) {
    u_int status, jx, val32, rc;

    rc = 1; // non-zero start value

    for (u_int i = 0; rc > 0; i++) {
        // lsb selects wheter upper or lower data register is used
        const u_int data01 = i & 1;
        // bit index for "Set DATAx" bit in Contol and Status Register
        const u_int databit = ROM_SET_DATA0 + data01;

        // read next dword
        rc = read(ifile, &val32, 4);

        if (rc == 0) {
            break; // done, no more data to write
        }
        RETURN_ON_ERR(rc < 0, "ERROR: Can't read image file\n");
        RETURN_ON_ERR(rc != 4, "ERROR: Could not get 4 bytes. Only got %x\n", rc);

        // wait for Set DATAx to become zero
        for (jx = 0; jx < LOOPNB; jx++) {
            usleep(POLL_US);

            if ((read_bit(fd, ctrl_reg, databit, &status) >= 0)
                    && (status == 0)) {
                break;
            }
        }

        RETURN_ON_ERR(jx == LOOPNB, "ERROR: SET_DATAx never go to zero\n");

        // write dword to DATAx register
        RETURN_ON_ERR(
            pci_cfg_write32(fd, DATAREG(data01), val32) < 0,
            "ERROR: Cant write DATAx register\n"
        );

        usleep(POLL_US);

        // trigger write
        // datasheet says, first two bytes should be uploaded together
        // before switching to an alternating upload order
        if (i == 1) {
            const u_int mask = (1 << ROM_SET_DATA0) | (1 << ROM_SET_DATA1);

            RETURN_ON_ERR(
                write_bitmask(fd, ctrl_reg, mask, mask) < 0,
                "ERROR: can't set SET_DATA01\n"
            );
        } else if (i > 1) {
            RETURN_ON_ERR(
                write_bit(fd, ctrl_reg, databit, 1) < 0,
                "ERROR: can't set SET_DATAx\n"
            );
        }
    }

    return 0; //Success!
}

int test_upload_result(int fd, u_int ctrl_reg) {
    u_int jx, status;

    // test result code
    for (jx = 0; jx < LOOPNB; jx++) {
        usleep(POLL_US);

        if (pci_cfg_read32(fd, ctrl_reg, &status) < 0) {
            status = -1;

            continue; // read might fail during update
        }

        if ((status & RESULT_BITMASK) == RESULT_SUCCESS) {
            break;
        }
    }

    RETURN_ON_ERR(
        (status & RESULT_BITMASK) != RESULT_SUCCESS,
        "ERROR: Writing firmware did not suceed, status register value: %x",
        status
    );

    return 0;
}

int write_eeprom(int fd, char *filename, unsigned int len) {

    int ifile;

    ifile = open(filename, O_RDWR);
    RETURN_ON_ERR(ifile < 0, "ERROR: cant open file image %s\n", filename);

    printf("STATUS: enabling EEPROM write\n");

    // enable access
    RETURN_ON_ERR(
        external_rom_access(fd, true) < 0,
        "ERROR: cant enable access to ROM \n"
    );

    sleep(1);

    printf("STATUS: performing EEPROM write\n");
    // perform upload
    if (do_upload(fd, ifile, EXT_ROM_CTRL_STATUS) < 0) {
        return -1;
    }

    sleep(1);

    printf("STATUS: finishing EEPROM write\n");
    // disable access
    RETURN_ON_ERR(
        external_rom_access(fd, false) < 0,
        "ERROR: cant DISABLE access to ROM\n"
    );

    sleep(1);

    printf("STATUS: confirming EEPROM write\n");

    // test result code
    if (test_upload_result(fd, EXT_ROM_CTRL_STATUS) < 0) {
        return -1;
    }

    return 0; //Success!
}

int write_firmware(int fd, char *filename, unsigned int len) {
    int ifile;

    ifile = open(filename, O_RDWR);
    RETURN_ON_ERR(ifile < 0, "ERROR: cant open file image %s\n", filename);

    printf("STATUS: enabling firmware upload\n");

    // enable access
    RETURN_ON_ERR(
        write_bit(fd, EXT_FW_DLOAD_CTRL_STATUS, FW_DLOAD_ENABLE, 1) < 0,
        "ERROR: cant enable access to firmware \n"
    );

    sleep(1);

    printf("STATUS: performing firmware upload\n");
    // perform upload
    if (do_upload(fd, ifile, EXT_FW_DLOAD_CTRL_STATUS) < 0) {
        return -1;
    }

    sleep(1);

    printf("STATUS: finishing firmware upload\n");

    // disable access
    RETURN_ON_ERR(
        write_bit(fd, EXT_FW_DLOAD_CTRL_STATUS, FW_DLOAD_ENABLE, 0) < 0,
        "ERROR: cant disable access to firmware \n"
    );

    sleep(1);

    printf("STATUS: confirming firmware upload\n");

    // test result code
    if (test_upload_result(fd, EXT_FW_DLOAD_CTRL_STATUS) < 0) {
        return -1;
    }

    return 0; //Success!
}

void usage() {

    printf("upd72020x-load: version 0.1\n");
    printf("usage: upd72020 -r -b bus -d dev -f fct -s -o outfile : read eeprom to file (size default is 0x10000 or 64KB)\n");
    //printf("usage: upd7202 -c -b -d -f -s -i outfile : check eeprom against file\n");
    printf("usage: upd72020 -w -b bus -d dev -f fct -i infile : write file to eeprom\n");
    printf("usage: upd72020 -u -b bus -d dev -f fct -i infile : upload file to firmware memory\n");
}

int main(int argc, char **argv) {
    unsigned char pcidevid_x1[] = { 0x12, 0x19, 0x14, 0x00 }; //uPD720201 vendor id = 1912 devid = 0014
    unsigned char pcidevid_x2[] = { 0x12, 0x19, 0x15, 0x00 }; //uPD720202 vendor id = 1912 devid = 0015
    unsigned char buf[100];
    unsigned int len;

    int i, fd;

    bool is_x1 = true, is_x2 = true;
    uint32_t bus, dev, fct;
    uint32_t size = 0x10000;
    uint32_t rflag = 0;
    uint32_t wflag = 0;
    uint32_t uflag = 0;
    uint32_t bflag, dflag, fflag, sflag, fileflag = 0;
    char *filename = NULL;
    char pcicfgfile[100];
    int c;
    opterr = 0;

    if (argc < 10) {
        usage();
        exit(1);
    }

    while ((c = getopt(argc, argv, "rwub:d:f:o:i:l:s:")) != -1) {
        switch (c) {
            case 'r':
                printf("Doing the reading\n");
                rflag = 1;
                break;
            case 'w':
                printf("Doing the writing\n");
                wflag = 1;
                break;
            case 'u':
                printf("Doing the upload\n");
                uflag = 1;
                break;
            case 'b':
                bflag = 1;
                bus = strtoul(optarg, NULL, 16); //hex numbers for size!!!
                break;
            case 'd':
                dflag = 1;
                dev = strtol(optarg, NULL, 16); //hex numbers for size!!!
                break;
            case 'f':
                fflag = 1;
                fct = strtol(optarg, NULL, 16); //hex numbers for size!!!
                break;
            case 's':
                sflag = 1;
                size = strtol(optarg, NULL, 16); //hex numbers for size!!!
                break;
            case 'o':
                fileflag = 1;
                filename = optarg; //hex numbers for size!!!
                break;
            case 'i':
                fileflag = 1;
                filename = optarg; //hex numbers for size!!!
                break;

            default:
                break;
        }
    }

    printf("bus = %x \n", bus);
    printf("dev = %x \n", dev);
    printf("fct = %x \n", fct);
    printf("fname = %s \n", filename);

    sprintf(pcicfgfile, "/sys/bus/pci/devices/0000:%02x:%02x.%01x/config",
            bus, dev, fct);

    fd = open(pcicfgfile, O_RDWR);
    if (fd < 0) {
        printf("ERROR: cant open PCI CONFIGURATION file %s\n", pcicfgfile);
        printf("FAILED\n");
        exit(1);
    }

// make sure the device is the right one.
    len = 4;
    read(fd, buf, len);

    for (i = 0; i < 4; i++) {
        if (pcidevid_x1[i] != buf[i]) {
            is_x1 = false;
        }        
        if (pcidevid_x2[i] != buf[i]) {
            is_x2 = false;
        }
    }
    
    if (is_x1) {
        printf("Found an UPD720201 chipset\n");
    } else if (is_x2) {
        printf("Found an UPD720202 chipset\n");
    } else {
        printf("ERROR: wrong vendorid/devid. Expected an UPD720201 or UPD720202 chip and this is not one!\n");
        printf("       reported vendorid/devid: %.2x%.2x:%.2x%.2x \n", buf[1], buf[0], buf[3], buf[2]);
        printf(FAILED);
        exit(1);
    }

    u_int fw_info, rom_info, rom_config;

    if (pci_cfg_read32(fd, EXT_FW_VERSION, &fw_info) < 0
            || pci_cfg_read32(fd, EXT_ROM_INFO_REG, &rom_info) < 0
            || pci_cfg_read32(fd, EXT_ROM_CONFIG_REG, &rom_config) < 0) {
        printf("ERROR: unable to read configuration registers\n");
        exit(1);
    }

    printf("got firmware version: %x\n", fw_info);

    if (eeprom_exists(fd) < 0) {
        printf("no EEPROM installed\n");

        if ((rflag | wflag) > 0) {
            printf("ERROR: can not perform action\n");
            exit(1);
        }
    } else {
        printf("EEPROM installed\n");
        printf("got rom_info: %x\n", rom_info);
        printf("got rom_config: %x\n", rom_config);

        rom_config = lookup_rompar(rom_info);

        if (rom_config != ROM_PARAM_INVALID) {
            printf("setting rom_config: %x\n", rom_config);

            if (pci_cfg_write32(fd, EXT_ROM_CONFIG_REG, rom_config) < 0) {
                printf("ERROR: failed to set ROM parameter register\n");
                exit(1);
            }
        } else {
            printf("unknown EEPROM, no parameters found\n");

            if ((rflag | wflag) > 0) {
                printf("ERROR: can not perform action\n");
                exit(1);
            }
        }
    }


    if (rflag == 1) {
        if (read_eeprom(fd, filename, size)) {
            printf(FAILED);
            exit(1);
        } else {
            printf(PASSED);
            exit(0);
        }

    }
    if (wflag == 1) {
        if (write_eeprom(fd, filename, size)) {
            printf(FAILED);
            exit(1);
        } else {
            printf(PASSED);
            exit(0);
        }
    }
    if (uflag == 1) {
        if (write_firmware(fd, filename, size)) {
            printf(FAILED);
            exit(1);
        } else {
            printf(PASSED);
            exit(0);
        }
    }

    printf("ERROR: Please specify an action. See help\n");
}
