# enumg
Generates c-source and -headers for enums containing "stringify" functions

## Command line
```
Usage: 
  enumg [file_1 [file_2 ... [file_n]]] [options]

options:
  -v        : print version and exit
  -V        : verbose output
  -h        : this screen
            : this screen (no arguments)
```

## Sample .ini file
```
c-header=hpp                       # header extension
c-source=cpp                       # source extension           
include-dir=include/               # where to put header
src-dir=src/                       # where to put source            
stringify-define=ENABLE_STRINGIFY  # only include stringify 
                                   #  functions if defined 

[FunctionCode]                     # name of the enum
type=enum                          # enum type (enum, enum class, ...)

field=FC_GET_EEPROM_INT            # field with incremental value
field=FC_SET_EEPROM_INT
field=FC_GET_AVAILABLE_MEMORY
field=PC_SET_SERIAL_NUMBER
field=PC_SET_NODE_IF_SERIAL_MATCHES
field=SQ_ACK
field=SQ_ERROR
field=SQ_REPEAT
field=SQ_CONFIRM_REPEAT
field=SQ_NO_DATA
field=FC_START_MSG=0xffff          # field with specific value

# can have multiple definitions per file;
# all are combined into one header/source pair
[other enum]
field=field1
field=field2
...
dield=fieldn

[yet another]
field=...
```

## c-functionality for above enum
```
#if defined(ENABLE_STRINGIFY)

//
// Return corresponding string if "value" is a valid field; NULL otherwise
//
const char *FunctionCodeToString(FunctionCode value);

//
// Return the zero-based index of "value" or negative 
// if "value" is not part of enum
//
int FunctionCodeToIndex(FunctionCode value);

//
// Return the enum value of "str". "str" must be a valid field. 
//
FunctionCode FunctionCodeFromString(const char *str);

//
// Return the total number of fields in enum
//
unsigned FunctionCodeValueCount();

//
// Return the enum value associated with "index" (index from xxxToIndex)
//
FunctionCode FunctionCodeFromIndex(unsigned index);

//
// Convert a string to enum value.
// Return 0 if OK, non-zero if failed.
//
// "str": the string to convert
// "presult": pointer to variable to store result if successful
//
int FunctionCodeFromString(const char *str, FunctionCode *presult, bool ignoreCase = false);

#endif 
```
