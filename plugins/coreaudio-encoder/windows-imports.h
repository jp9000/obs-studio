#define NO_MIN_MAX 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShlObj.h>

#include <util/dstr.h>

typedef unsigned long UInt32;
typedef signed long SInt32;
typedef signed long long SInt64;
typedef double Float64;

typedef SInt32 OSStatus;
typedef unsigned char Boolean;

typedef UInt32 AudioFormatPropertyID;

enum { kVariableLengthArray = 1,
};

struct OpaqueAudioConverter;
typedef struct OpaqueAudioConverter *AudioConverterRef;
typedef UInt32 AudioConverterPropertyID;

struct AudioValueRange {
	Float64 mMinimum;
	Float64 mMaximum;
};
typedef struct AudioValueRange AudioValueRange;

struct AudioBuffer {
	UInt32 mNumberChannels;
	UInt32 mDataByteSize;
	void *mData;
};
typedef struct AudioBuffer AudioBuffer;

struct AudioBufferList {
	UInt32 mNumberBuffers;
	AudioBuffer mBuffers[kVariableLengthArray];
};
typedef struct AudioBufferList AudioBufferList;

struct AudioStreamBasicDescription {
	Float64 mSampleRate;
	UInt32 mFormatID;
	UInt32 mFormatFlags;
	UInt32 mBytesPerPacket;
	UInt32 mFramesPerPacket;
	UInt32 mBytesPerFrame;
	UInt32 mChannelsPerFrame;
	UInt32 mBitsPerChannel;
	UInt32 mReserved;
};
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;

struct AudioStreamPacketDescription {
	SInt64 mStartOffset;
	UInt32 mVariableFramesInPacket;
	UInt32 mDataByteSize;
};
typedef struct AudioStreamPacketDescription AudioStreamPacketDescription;

typedef UInt32 AudioChannelLabel;
typedef UInt32 AudioChannelLayoutTag;

struct AudioChannelDescription {
	AudioChannelLabel mChannelLabel;
	UInt32 mChannelFlags;
	float mCoordinates[3];
};
typedef struct AudioChannelDescription AudioChannelDescription;

struct AudioChannelLayout {
	AudioChannelLayoutTag mChannelLayoutTag;
	UInt32 mChannelBitmap;
	UInt32 mNumberChannelDescriptions;
	AudioChannelDescription mChannelDescriptions[kVariableLengthArray];
};
typedef struct AudioChannelLayout AudioChannelLayout;

typedef OSStatus (*AudioConverterComplexInputDataProc)(
	AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets,
	AudioBufferList *ioData,
	AudioStreamPacketDescription **outDataPacketDescription,
	void *inUserData);

enum { kAudioCodecPropertyNameCFString = 'lnam',
       kAudioCodecPropertyManufacturerCFString = 'lmak',
       kAudioCodecPropertyFormatCFString = 'lfor',
       //kAudioCodecPropertyHasVariablePacketByteSizes          = 'vpk?',
       kAudioCodecPropertySupportedInputFormats = 'ifm#',
       kAudioCodecPropertySupportedOutputFormats = 'ofm#',
       kAudioCodecPropertyAvailableInputSampleRates = 'aisr',
       kAudioCodecPropertyAvailableOutputSampleRates = 'aosr',
       kAudioCodecPropertyAvailableBitRateRange = 'abrt',
       kAudioCodecPropertyMinimumNumberInputPackets = 'mnip',
       kAudioCodecPropertyMinimumNumberOutputPackets = 'mnop',
       kAudioCodecPropertyAvailableNumberChannels = 'cmnc',
       kAudioCodecPropertyDoesSampleRateConversion = 'lmrc',
       kAudioCodecPropertyAvailableInputChannelLayoutTags = 'aicl',
       kAudioCodecPropertyAvailableOutputChannelLayoutTags = 'aocl',
       kAudioCodecPropertyInputFormatsForOutputFormat = 'if4o',
       kAudioCodecPropertyOutputFormatsForInputFormat = 'of4i',
       kAudioCodecPropertyFormatInfo = 'acfi',
};

enum { kAudioCodecPropertyInputBufferSize = 'tbuf',
       kAudioCodecPropertyPacketFrameSize = 'pakf',
       kAudioCodecPropertyMaximumPacketByteSize = 'pakb',
       kAudioCodecPropertyCurrentInputFormat = 'ifmt',
       kAudioCodecPropertyCurrentOutputFormat = 'ofmt',
       kAudioCodecPropertyMagicCookie = 'kuki',
       kAudioCodecPropertyUsedInputBufferSize = 'ubuf',
       kAudioCodecPropertyIsInitialized = 'init',
       kAudioCodecPropertyCurrentTargetBitRate = 'brat',
       kAudioCodecPropertyCurrentInputSampleRate = 'cisr',
       kAudioCodecPropertyCurrentOutputSampleRate = 'cosr',
       kAudioCodecPropertyQualitySetting = 'srcq',
       kAudioCodecPropertyApplicableBitRateRange = 'brta',
       kAudioCodecPropertyApplicableInputSampleRates = 'isra',
       kAudioCodecPropertyApplicableOutputSampleRates = 'osra',
       kAudioCodecPropertyPaddedZeros = 'pad0',
       kAudioCodecPropertyPrimeMethod = 'prmm',
       kAudioCodecPropertyPrimeInfo = 'prim',
       kAudioCodecPropertyCurrentInputChannelLayout = 'icl ',
       kAudioCodecPropertyCurrentOutputChannelLayout = 'ocl ',
       kAudioCodecPropertySettings = 'acs ',
       kAudioCodecPropertyFormatList = 'acfl',
       kAudioCodecPropertyBitRateControlMode = 'acbf',
       kAudioCodecPropertySoundQualityForVBR = 'vbrq',
       kAudioCodecPropertyMinimumDelayMode = 'mdel' };

enum { kAudioCodecBitRateControlMode_Constant = 0,
       kAudioCodecBitRateControlMode_LongTermAverage = 1,
       kAudioCodecBitRateControlMode_VariableConstrained = 2,
       kAudioCodecBitRateControlMode_Variable = 3,
};

enum { kAudioFormatLinearPCM = 'lpcm',
       kAudioFormatAC3 = 'ac-3',
       kAudioFormat60958AC3 = 'cac3',
       kAudioFormatAppleIMA4 = 'ima4',
       kAudioFormatMPEG4AAC = 'aac ',
       kAudioFormatMPEG4CELP = 'celp',
       kAudioFormatMPEG4HVXC = 'hvxc',
       kAudioFormatMPEG4TwinVQ = 'twvq',
       kAudioFormatMACE3 = 'MAC3',
       kAudioFormatMACE6 = 'MAC6',
       kAudioFormatULaw = 'ulaw',
       kAudioFormatALaw = 'alaw',
       kAudioFormatQDesign = 'QDMC',
       kAudioFormatQDesign2 = 'QDM2',
       kAudioFormatQUALCOMM = 'Qclp',
       kAudioFormatMPEGLayer1 = '.mp1',
       kAudioFormatMPEGLayer2 = '.mp2',
       kAudioFormatMPEGLayer3 = '.mp3',
       kAudioFormatTimeCode = 'time',
       kAudioFormatMIDIStream = 'midi',
       kAudioFormatParameterValueStream = 'apvs',
       kAudioFormatAppleLossless = 'alac',
       kAudioFormatMPEG4AAC_HE = 'aach',
       kAudioFormatMPEG4AAC_LD = 'aacl',
       kAudioFormatMPEG4AAC_ELD = 'aace',
       kAudioFormatMPEG4AAC_ELD_SBR = 'aacf',
       kAudioFormatMPEG4AAC_ELD_V2 = 'aacg',
       kAudioFormatMPEG4AAC_HE_V2 = 'aacp',
       kAudioFormatMPEG4AAC_Spatial = 'aacs',
       kAudioFormatAMR = 'samr',
       kAudioFormatAudible = 'AUDB',
       kAudioFormatiLBC = 'ilbc',
       kAudioFormatDVIIntelIMA = 0x6D730011,
       kAudioFormatMicrosoftGSM = 0x6D730031,
       kAudioFormatAES3 = 'aes3',
};

enum { kAudioFormatFlagIsFloat = (1L << 0),
       kAudioFormatFlagIsBigEndian = (1L << 1),
       kAudioFormatFlagIsSignedInteger = (1L << 2),
       kAudioFormatFlagIsPacked = (1L << 3),
       kAudioFormatFlagIsAlignedHigh = (1L << 4),
       kAudioFormatFlagIsNonInterleaved = (1L << 5),
       kAudioFormatFlagIsNonMixable = (1L << 6),
       kAudioFormatFlagsAreAllClear = (1L << 31),

       kLinearPCMFormatFlagIsFloat = kAudioFormatFlagIsFloat,
       kLinearPCMFormatFlagIsBigEndian = kAudioFormatFlagIsBigEndian,
       kLinearPCMFormatFlagIsSignedInteger = kAudioFormatFlagIsSignedInteger,
       kLinearPCMFormatFlagIsPacked = kAudioFormatFlagIsPacked,
       kLinearPCMFormatFlagIsAlignedHigh = kAudioFormatFlagIsAlignedHigh,
       kLinearPCMFormatFlagIsNonInterleaved = kAudioFormatFlagIsNonInterleaved,
       kLinearPCMFormatFlagIsNonMixable = kAudioFormatFlagIsNonMixable,
       kLinearPCMFormatFlagsAreAllClear = kAudioFormatFlagsAreAllClear,

       kAppleLosslessFormatFlag_16BitSourceData = 1,
       kAppleLosslessFormatFlag_20BitSourceData = 2,
       kAppleLosslessFormatFlag_24BitSourceData = 3,
       kAppleLosslessFormatFlag_32BitSourceData = 4,
};

enum { kAudioFormatFlagsNativeEndian = 0 };

enum {
	// AudioStreamBasicDescription structure properties
	kAudioFormatProperty_FormatInfo = 'fmti',
	kAudioFormatProperty_FormatName = 'fnam',
	kAudioFormatProperty_EncodeFormatIDs = 'acof',
	kAudioFormatProperty_DecodeFormatIDs = 'acif',
	kAudioFormatProperty_FormatList = 'flst',
	kAudioFormatProperty_ASBDFromESDS = 'essd',
	kAudioFormatProperty_ChannelLayoutFromESDS = 'escl',
	kAudioFormatProperty_OutputFormatList = 'ofls',
	kAudioFormatProperty_Encoders = 'aven',
	kAudioFormatProperty_Decoders = 'avde',
	kAudioFormatProperty_FormatIsVBR = 'fvbr',
	kAudioFormatProperty_FormatIsExternallyFramed = 'fexf',
	kAudioFormatProperty_AvailableEncodeBitRates = 'aebr',
	kAudioFormatProperty_AvailableEncodeSampleRates = 'aesr',
	kAudioFormatProperty_AvailableEncodeChannelLayoutTags = 'aecl',
	kAudioFormatProperty_AvailableEncodeNumberChannels = 'avnc',
	kAudioFormatProperty_ASBDFromMPEGPacket = 'admp',
	//
	// AudioChannelLayout structure properties
	kAudioFormatProperty_BitmapForLayoutTag = 'bmtg',
	kAudioFormatProperty_MatrixMixMap = 'mmap',
	kAudioFormatProperty_ChannelMap = 'chmp',
	kAudioFormatProperty_NumberOfChannelsForLayout = 'nchm',
	kAudioFormatProperty_ValidateChannelLayout = 'vacl',
	kAudioFormatProperty_ChannelLayoutForTag = 'cmpl',
	kAudioFormatProperty_TagForChannelLayout = 'cmpt',
	kAudioFormatProperty_ChannelLayoutName = 'lonm',
	kAudioFormatProperty_ChannelLayoutSimpleName = 'lsnm',
	kAudioFormatProperty_ChannelLayoutForBitmap = 'cmpb',
	kAudioFormatProperty_ChannelName = 'cnam',
	kAudioFormatProperty_ChannelShortName = 'csnm',
	kAudioFormatProperty_TagsForNumberOfChannels = 'tagc',
	kAudioFormatProperty_PanningMatrix = 'panm',
	kAudioFormatProperty_BalanceFade = 'balf',
	//
	// ID3 tag (MP3 metadata) properties
	kAudioFormatProperty_ID3TagSize = 'id3s',
	kAudioFormatProperty_ID3TagToDictionary = 'id3d',
};

enum { kAudioConverterPropertyMinimumInputBufferSize = 'mibs',
       kAudioConverterPropertyMinimumOutputBufferSize = 'mobs',
       kAudioConverterPropertyMaximumInputBufferSize = 'xibs',
       kAudioConverterPropertyMaximumInputPacketSize = 'xips',
       kAudioConverterPropertyMaximumOutputPacketSize = 'xops',
       kAudioConverterPropertyCalculateInputBufferSize = 'cibs',
       kAudioConverterPropertyCalculateOutputBufferSize = 'cobs',
       kAudioConverterPropertyInputCodecParameters = 'icdp',
       kAudioConverterPropertyOutputCodecParameters = 'ocdp',
       kAudioConverterSampleRateConverterAlgorithm = 'srci',
       kAudioConverterSampleRateConverterComplexity = 'srca',
       kAudioConverterSampleRateConverterQuality = 'srcq',
       kAudioConverterSampleRateConverterInitialPhase = 'srcp',
       kAudioConverterCodecQuality = 'cdqu',
       kAudioConverterPrimeMethod = 'prmm',
       kAudioConverterPrimeInfo = 'prim',
       kAudioConverterChannelMap = 'chmp',
       kAudioConverterDecompressionMagicCookie = 'dmgc',
       kAudioConverterCompressionMagicCookie = 'cmgc',
       kAudioConverterEncodeBitRate = 'brat',
       kAudioConverterEncodeAdjustableSampleRate = 'ajsr',
       kAudioConverterInputChannelLayout = 'icl ',
       kAudioConverterOutputChannelLayout = 'ocl ',
       kAudioConverterApplicableEncodeBitRates = 'aebr',
       kAudioConverterAvailableEncodeBitRates = 'vebr',
       kAudioConverterApplicableEncodeSampleRates = 'aesr',
       kAudioConverterAvailableEncodeSampleRates = 'vesr',
       kAudioConverterAvailableEncodeChannelLayoutTags = 'aecl',
       kAudioConverterCurrentOutputStreamDescription = 'acod',
       kAudioConverterCurrentInputStreamDescription = 'acid',
       kAudioConverterPropertySettings = 'acps',
       kAudioConverterPropertyBitDepthHint = 'acbd',
       kAudioConverterPropertyFormatList = 'flst',
};

enum { kAudioConverterQuality_Max = 0x7F,
       kAudioConverterQuality_High = 0x60,
       kAudioConverterQuality_Medium = 0x40,
       kAudioConverterQuality_Low = 0x20,
       kAudioConverterQuality_Min = 0,
};

enum { kAudio_UnimplementedError = -4,
       kAudio_FileNotFoundError = -43,
       kAudio_FilePermissionError = -54,
       kAudio_TooManyFilesOpenError = -42,
       kAudio_BadFilePathError = '!pth', // 0x21707468, 561017960
       kAudio_ParamError = -50,
       kAudio_MemFullError = -108,

       kAudioConverterErr_FormatNotSupported = 'fmt?',
       kAudioConverterErr_OperationNotSupported = 0x6F703F3F,
       // 'op??', integer used because of trigraph
       kAudioConverterErr_PropertyNotSupported = 'prop',
       kAudioConverterErr_InvalidInputSize = 'insz',
       kAudioConverterErr_InvalidOutputSize = 'otsz',
       // e.g. byte size is not a multiple of the frame size
       kAudioConverterErr_UnspecifiedError = 'what',
       kAudioConverterErr_BadPropertySizeError = '!siz',
       kAudioConverterErr_RequiresPacketDescriptionsError = '!pkd',
       kAudioConverterErr_InputSampleRateOutOfRange = '!isr',
       kAudioConverterErr_OutputSampleRateOutOfRange = '!osr',
};

typedef OSStatus (*AudioConverterNew_t)(
	const AudioStreamBasicDescription *inSourceFormat,
	const AudioStreamBasicDescription *inDestinationFormat,
	AudioConverterRef *outAudioConverter);

typedef OSStatus (*AudioConverterDispose_t)(AudioConverterRef inAudioConverter);

typedef OSStatus (*AudioConverterReset_t)(AudioConverterRef inAudioConverter);

typedef OSStatus (*AudioConverterGetProperty_t)(
	AudioConverterRef inAudioConverter,
	AudioConverterPropertyID inPropertyID, UInt32 *ioPropertyDataSize,
	void *outPropertyData);

typedef OSStatus (*AudioConverterGetPropertyInfo_t)(
	AudioConverterRef inAudioConverter,
	AudioConverterPropertyID inPropertyID, UInt32 *outSize,
	Boolean *outWritable);

typedef OSStatus (*AudioConverterSetProperty_t)(
	AudioConverterRef inAudioConverter,
	AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize,
	const void *inPropertyData);

typedef OSStatus (*AudioConverterFillComplexBuffer_t)(
	AudioConverterRef inAudioConverter,
	AudioConverterComplexInputDataProc inInputDataProc,
	void *inInputDataProcUserData, UInt32 *ioOutputDataPacketSize,
	AudioBufferList *outOutputData,
	AudioStreamPacketDescription *outPacketDescription);

typedef OSStatus (*AudioFormatGetProperty_t)(AudioFormatPropertyID inPropertyID,
					     UInt32 inSpecifierSize,
					     const void *inSpecifier,
					     UInt32 *ioPropertyDataSize,
					     void *outPropertyData);

typedef OSStatus (*AudioFormatGetPropertyInfo_t)(
	AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize,
	const void *inSpecifier, UInt32 *outPropertyDataSize);

static AudioConverterNew_t AudioConverterNew = NULL;
static AudioConverterDispose_t AudioConverterDispose = NULL;
static AudioConverterReset_t AudioConverterReset = NULL;
static AudioConverterGetProperty_t AudioConverterGetProperty = NULL;
static AudioConverterGetPropertyInfo_t AudioConverterGetPropertyInfo = NULL;
static AudioConverterSetProperty_t AudioConverterSetProperty = NULL;
static AudioConverterFillComplexBuffer_t AudioConverterFillComplexBuffer = NULL;
static AudioFormatGetProperty_t AudioFormatGetProperty = NULL;
static AudioFormatGetPropertyInfo_t AudioFormatGetPropertyInfo = NULL;

static HMODULE audio_toolbox = NULL;

static void release_lib(void)
{
	if (audio_toolbox) {
		FreeLibrary(audio_toolbox);
		audio_toolbox = NULL;
	}
}

static bool load_from_shell_path(REFKNOWNFOLDERID rfid, const wchar_t *subpath)
{
	wchar_t *sh_path;
	if (SHGetKnownFolderPath(rfid, 0, NULL, &sh_path) != S_OK) {
		CA_LOG(LOG_WARNING, "Could not retrieve shell path");
		return false;
	}

	wchar_t path[MAX_PATH];
	_snwprintf(path, MAX_PATH, L"%s\\%s", sh_path, subpath);
	CoTaskMemFree(sh_path);

	SetDllDirectory(path);
	audio_toolbox = LoadLibraryW(L"CoreAudioToolbox.dll");
	SetDllDirectory(nullptr);

	return !!audio_toolbox;
}

static bool load_lib(void)
{
	/* -------------------------------------------- */
	/* attempt to load from path                    */

	audio_toolbox = LoadLibraryW(L"CoreAudioToolbox.dll");
	if (!!audio_toolbox)
		return true;

	/* -------------------------------------------- */
	/* attempt to load from known install locations */

	struct path_list_t {
		REFKNOWNFOLDERID rfid;
		const wchar_t *subpath;
	};

	path_list_t path_list[] = {
		{FOLDERID_ProgramFilesCommon,
		 L"Apple\\Apple Application Support"},
		{FOLDERID_ProgramFiles, L"iTunes"},
	};

	for (auto &val : path_list) {
		if (load_from_shell_path(val.rfid, val.subpath)) {
			return true;
		}
	}

	return false;
}

static void unload_core_audio(void)
{
	AudioConverterNew = NULL;
	AudioConverterDispose = NULL;
	AudioConverterReset = NULL;
	AudioConverterGetProperty = NULL;
	AudioConverterGetPropertyInfo = NULL;
	AudioConverterSetProperty = NULL;
	AudioConverterFillComplexBuffer = NULL;
	AudioFormatGetProperty = NULL;
	AudioFormatGetPropertyInfo = NULL;

	release_lib();
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4706)
#endif
static bool load_core_audio(void)
{
	if (!load_lib())
		return false;

#define LOAD_SYM_FROM_LIB(sym, lib, dll)                                   \
	if (!(sym = (sym##_t)GetProcAddress(lib, #sym))) {                 \
		DWORD err = GetLastError();                                \
		CA_LOG(LOG_ERROR,                                          \
		       "Couldn't load " #sym " from " dll ": %lu (0x%lx)", \
		       err, err);                                          \
		goto unload_everything;                                    \
	}

#define LOAD_SYM(sym) \
	LOAD_SYM_FROM_LIB(sym, audio_toolbox, "CoreAudioToolbox.dll")
	LOAD_SYM(AudioConverterNew);
	LOAD_SYM(AudioConverterDispose);
	LOAD_SYM(AudioConverterReset);
	LOAD_SYM(AudioConverterGetProperty);
	LOAD_SYM(AudioConverterGetPropertyInfo);
	LOAD_SYM(AudioConverterSetProperty);
	LOAD_SYM(AudioConverterFillComplexBuffer);
	LOAD_SYM(AudioFormatGetProperty);
	LOAD_SYM(AudioFormatGetPropertyInfo);
#undef LOAD_SYM

	return true;

unload_everything:
	unload_core_audio();

	return false;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
