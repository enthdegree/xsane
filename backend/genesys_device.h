/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Povilas Kanapickas <povilas@radix.lt>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#ifndef BACKEND_GENESYS_DEVICE_H
#define BACKEND_GENESYS_DEVICE_H

#include "genesys_calibration.h"
#include "genesys_buffer.h"
#include "genesys_enums.h"
#include "genesys_motor.h"
#include "genesys_settings.h"
#include "genesys_sensor.h"
#include "genesys_register.h"
#include "genesys_sanei.h"
#include <vector>

struct Genesys_Gpo
{
    Genesys_Gpo() = default;

    // Genesys_Gpo
    uint8_t gpo_id = 0;

    /*  GL646 and possibly others:
        - have the value registers at 0x66 and 0x67
        - have the enable registers at 0x68 and 0x69

        GL841, GL842, GL843, GL846, GL848 and possibly others:
        - have the value registers at 0x6c and 0x6d.
        - have the enable registers at 0x6e and 0x6f.
    */
    GenesysRegisterSettingSet regs;
};

struct Genesys_Command_Set;

/** @brief structure to describe a scanner model
 * This structure describes a model. It is composed of information on the
 * sensor, the motor, scanner geometry and flags to drive operation.
 */
struct Genesys_Model
{
    Genesys_Model() = default;

    const char* name = nullptr;
    const char* vendor = nullptr;
    const char* model = nullptr;
    unsigned model_id = 0;

    AsicType asic_type = AsicType::UNKNOWN;

    // possible x resolutions
    std::vector<unsigned> xdpi_values;
    // possible y resolutions
    std::vector<unsigned> ydpi_values;

    // possible depths in gray mode
    std::vector<unsigned> bpp_gray_values;
    // possible depths in color mode
    std::vector<unsigned> bpp_color_values;

    // All offsets below are with respect to the sensor home position

    // Start of scan area in mm
    SANE_Fixed x_offset = 0;

    // Start of scan area in mm (Amount of feeding needed to get to the medium)
    SANE_Fixed y_offset = 0;

    // Size of scan area in mm
    SANE_Fixed x_size = 0;

    // Size of scan area in mm
    SANE_Fixed y_size = 0;

    // Start of white strip in mm
    SANE_Fixed y_offset_calib = 0;

    // Start of black mark in mm
    SANE_Fixed x_offset_mark = 0;

    // Start of scan area in TA mode in mm
    SANE_Fixed x_offset_ta = 0;

    // Start of scan area in TA mode in mm
    SANE_Fixed y_offset_ta = 0;

    // Size of scan area in TA mode in mm
    SANE_Fixed x_size_ta = 0;

    // Size of scan area in TA mode in mm
    SANE_Fixed y_size_ta = 0;

    // The position of the sensor when it's aligned with the lamp for transparency scanning
    SANE_Fixed y_offset_sensor_to_ta = 0;

    // Start of white strip in TA mode in mm
    SANE_Fixed y_offset_calib_ta = 0;

    // Size of scan area after paper sensor stop sensing document in mm
    SANE_Fixed post_scan = 0;

    // Amount of feeding needed to eject document after finishing scanning in mm
    SANE_Fixed eject_feed = 0;

    // Line-distance correction (in pixel at optical_ydpi) for CCD scanners
    SANE_Int ld_shift_r = 0;
    SANE_Int ld_shift_g = 0;
    SANE_Int ld_shift_b = 0;

    // Order of the CCD/CIS colors
    Genesys_Color_Order line_mode_color_order = COLOR_ORDER_RGB;

    // Is this a CIS or CCD scanner?
    SANE_Bool is_cis = false;

    // Is this sheetfed scanner?
    SANE_Bool is_sheetfed = false;

    // sensor type
    SANE_Int ccd_type = 0;
    // Digital-Analog converter type (TODO: rename to ADC)
    SANE_Int dac_type = 0;
    // General purpose output type
    SANE_Int gpo_type = 0;
    // stepper motor type
    SANE_Int motor_type = 0;

    // Which hacks are needed for this scanner?
    SANE_Word flags = 0;

    // Button flags, described existing buttons for the model
    SANE_Word buttons = 0;

    // how many lines are used for shading calibration
    SANE_Int shading_lines = 0;
    // how many lines are used for shading calibration in TA mode
    SANE_Int shading_ta_lines = 0;
    // how many lines are used to search start position
    SANE_Int search_lines = 0;

    std::vector<unsigned> get_resolutions() const
    {
        std::vector<unsigned> ret;
        std::copy(xdpi_values.begin(), xdpi_values.end(), std::back_inserter(ret));
        std::copy(ydpi_values.begin(), ydpi_values.end(), std::back_inserter(ret));
        // sort in decreasing order

        std::sort(ret.begin(), ret.end(), std::greater<unsigned>());
        ret.erase(std::unique(ret.begin(), ret.end()), ret.end());
        return ret;
    }
};

// Describes the geometry of the raw data coming out of the scanner for desegmentation.
struct DesegmentationState
{
    // The number of bytes to skip at start of line. Currently it's always zero.
    unsigned skip_bytes = 0;

    // The number of "even" pixels to scan. This corresponds to the number of pixels that will be
    // scanned from a single segment
    unsigned pixel_groups = 0;

    // Total bytes in a channel received from a scanner
    unsigned raw_channel_bytes = 0;

    // Total bytes in a line received from a scanner
    unsigned raw_line_bytes = 0;

    // The current byte during desegmentation process
    unsigned curr_byte = 0;
};

/**
 * Describes the current device status for the backend
 * session. This should be more accurately called
 * Genesys_Session .
 */
struct Genesys_Device
{
    Genesys_Device() = default;
    ~Genesys_Device();

    using Calibration = std::vector<Genesys_Calibration_Cache>;

    // frees commonly used data
    void clear();

    UsbDevice usb_dev;
    SANE_Word vendorId = 0;			/**< USB vendor identifier */
    SANE_Word productId = 0;			/**< USB product identifier */

    // USB mode:
    // 0: not set
    // 1: USB 1.1
    // 2: USB 2.0
    SANE_Int usb_mode = 0;

    std::string file_name;
    std::string calib_file;

    // if enabled, no calibration data will be loaded or saved to files
    SANE_Int force_calibration = 0;
    // if enabled, will ignore the scan offsets and start scanning at true origin. This allows
    // acquiring the positions of the black and white strips and the actual scan area
    bool ignore_offsets = false;

    Genesys_Model *model = nullptr;

    // pointers to low level functions
    Genesys_Command_Set* cmd_set = nullptr;

    Genesys_Register_Set reg;
    Genesys_Register_Set calib_reg;
    Genesys_Settings settings;
    Genesys_Frontend frontend, frontend_initial;
    Genesys_Gpo gpo;
    Genesys_Motor motor;
    uint8_t  control[6] = {};

    size_t average_size = 0;
    // number of pixels used during shading calibration
    size_t calib_pixels = 0;
    // number of lines used during shading calibration
    size_t calib_lines = 0;
    size_t calib_channels = 0;
    size_t calib_resolution = 0;
     // bytes to read from USB when calibrating. If 0, this is not set
    size_t calib_total_bytes_to_read = 0;
    // certain scanners support much higher resolution when scanning transparency, but we can't
    // read whole width of the scanner as a single line at that resolution. Thus for stuff like
    // calibration we want to read only the possible calibration area.
    size_t calib_pixels_offset = 0;

    // gamma overrides. If a respective array is not empty then it means that the gamma for that
    // color is overridden.
    std::vector<uint16_t> gamma_override_tables[3];

    std::vector<uint16_t> white_average_data;
    std::vector<uint16_t> dark_average_data;

    SANE_Bool already_initialized = 0;
    SANE_Int scanhead_position_in_steps = 0;

    SANE_Bool read_active = 0;
    // signal wether the park command has been issued
    SANE_Bool parking = 0;

    // for sheetfed scanner's, is TRUE when there is a document in the scanner
    SANE_Bool document = 0;

    SANE_Bool needs_home_ta = 0;

    Genesys_Buffer read_buffer;
    Genesys_Buffer lines_buffer;
    Genesys_Buffer shrink_buffer;
    Genesys_Buffer out_buffer;

    // buffer for digital lineart from gray data
    Genesys_Buffer binarize_buffer;
    // local buffer for gray data during dynamix lineart
    Genesys_Buffer local_buffer;

    // bytes to read from desegmentation step. This is not the same as physical bytes read from
    // scanners, see `deseg.raw_line_bytes` which corresponds to this information on certain
    // scanners.
    size_t read_bytes_left_after_deseg = 0;

    // total bytes read sent to frontend
    size_t total_bytes_read = 0;
    // total bytes read to be sent to frontend
    size_t total_bytes_to_read = 0;

    DesegmentationState deseg;

    // contains the real used values
    Genesys_Current_Setup current_setup;
    // contains computed data for the current setup
    ScanSession session;

    // look up table used in dynamic rasterization
    unsigned char lineart_lut[256] = {};

    Calibration calibration_cache;

    // used red line-distance shift
    SANE_Int ld_shift_r = 0;
    // used green line-distance shift
    SANE_Int ld_shift_g = 0;
    // used blue line-distance shift
    SANE_Int ld_shift_b = 0;
    // number of lines used in line interpolation
    int line_interp = 0;
    // number of scan lines used during scan
    int line_count = 0;

    // array describing the order of the sub-segments of the sensor
    std::vector<unsigned> segment_order;

    // buffer to handle even/odd data
    Genesys_Buffer oe_buffer = {};

    // when true the scanned picture is first buffered to allow software image enhancements
    SANE_Bool buffer_image = 0;

    // image buffer where the scanned picture is stored
    std::vector<uint8_t> img_buffer;

    // binary logger file
    FILE *binary = nullptr;

    // A snapshot of the last known physical state of the device registers. This variable is updated
    // whenever a register is written or read to the scanner.
    Genesys_Register_Set physical_regs;

    uint8_t read_register(uint16_t address);
    void write_register(uint16_t address, uint8_t value);
    void write_registers(Genesys_Register_Set& regs);

private:
    void update_register_state(uint16_t address, uint8_t value);
};

void apply_reg_settings_to_device(Genesys_Device& dev, const GenesysRegisterSettingSet& regs);

#endif