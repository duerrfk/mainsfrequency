This is an appliance and tool chain to capture and analyze mains frequency (aka utility frequency).

# Architecure

The appliance consist of the following major hardware components:

```
Transformer --(sine wave)--> half-wave rectifier --(half sine wave)--> Schmitt-Trigger --(square wave)--> Microcontroller --(samples via SLIP)--> Computer --(stream of TLV records)--> raw file 
                                                                                                               ^ 
                                                                                                               |
GPS Device <-------------------------------1-pps signal---------------------------------------------------------
```

As microcontroller, we use an ARDUINO DUE and its timer capture feature to precisely measure sine wave periods and the period of the reference 1-pps signal from GPS. 

The transmission from the microcontroller to the computer (we use a Raspberry Pi) uses serial over USB, the SLIP protocol to send packets over a serial byte stream, and CRC for checking the integrity of packets.

# Pipeline for Processing Stream Data

A stream of data is processed in a pipeline of filters using the popular UNIX pipe concept.
A filter reads its input from stdin and writes its output to stdout.
Filters can be concatenated as required.

* The `pkt-to-tlv-stream` application receives data from the microcontroller and acts as stream source. Use `>` to redirect the stream to a file. 
* The `cat` application can be used to start a stream from a recorded file.
* The `tee` application can be used to fork a data stream.
* The application `sink-display` can display the values of a stream.

The filters are described below.

# Recording RAW Data Records (TLV Files)

Raw data is recorded by the `pkt-to-tlv-stream` application.
This application receives sample from the microcontroller over a serial connection using the SLIP protocol.

Raw data is recorded in binary format (Little Endian) as a stream of type-length-value (TLV) records.
Type is a uint16 number; length is a uint16 number defining the length of the value(s ) in bytes.

The interpretation of the value(s) depends on the type. The following types are defined:

* SAMPLES record (type 0): a batch of n = length/4 uint32 values defining the number of clock ticks of n consecutive waves. The clock ticks at a nominal rate of 42 MHz. 
* ONEPPS record (type 1): a single uint32 value defining the number of clock ticks per second, calibrated by a 1-pps signal from a GPS device.
* WALLCLOCKTIME (type 2): a single uint64 value defining nanoseconds since the UNIX epoch (00:00:00 UTC, Jan. 1, 1970) referencing the stream roughly to wallclock (real) time. Note that this is just a rough reference to real-time. In particular, not every sample is timestamped, but wallclock timestamps are inserted into the stream every second.

# Converting TLV Files to CSV Files

Clean data, published as comma-separated values (CSV) files, is created from raw data as follows:

A CSV file can be produced from raw TLV records by the filter `filter-convert_to_csv`.

The CSV file contains the following fields:

* f_mains: mains frequency as captured by uncalibrated 42 MHz clock. The crystal oscillator can be expected to have an accuracy of 30 ppm over the full temperatur range (the calibration shows an accuracy better than 20 ppm, which is typical for for crystal oscillators at room temperature). 
* f_mains_syncd: mains frequency, calibrated to 1-pps signal of GPS device (using the value f_clk_synd as described next).
* f_clk_syncd: calibrated clock frequency of microcontroller using 1-pps signal (nominal 42 MHz).
* clk_accuracy_ppm: estimated clock accuracy in ppm (relevant when interpreting f_mains instead of f_mains_syncd).
* t_wallclock: wallclock time in nanoseconds since UNIX epoch (00:00:00 UTC, Jan 1, 1970). Note that this timestamp is only taken once per second to roughly reference the samples to wallclock time. Therefore, several samples have the same wallclock timestamp! Still, it is useful for selecting all samples of one day, hour, minute, etc.
* t_wallclock_str: string representation of t_wallclock value (in UTC).

# Sanity Checking TLV Records

The filter `filter-sanitycheck_onepps` checks a TLV stream for obviously incorrect ONEPPS records.
In seldom cases, the GPS device might not output a 1-pps signal for a short period.
In these cases, the 1-pps value deviates significantly from the nominal value of 42 MHz (the nominal clock frequency of the microcontroller).
Such 1-pps records are removed by the filter.

# Selecting TLV Records from a Time Window

The filter `filter-timewnd` can extract all TLV records within a given time window. 
