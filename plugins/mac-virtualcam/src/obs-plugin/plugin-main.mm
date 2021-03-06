#include <obs-module.h>
#include <obs.hpp>
#include <pthread.h>
#include <QMainWindow.h>
#include <QAction.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AppKit/AppKit.h>
#include "OBSDALMachServer.h"
#include "Defines.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("mac-virtualcam", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "macOS virtual webcam output";
}

obs_output_t *outputRef;
obs_video_info videoInfo;
static OBSDALMachServer *sMachServer;

static bool check_dal_plugin()
{
	NSFileManager *fileManager = [NSFileManager defaultManager];

	NSString *dalPluginDestinationPath =
		@"/Library/CoreMediaIO/Plug-Ins/DAL/";
	NSString *dalPluginFileName =
		@"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin";

	BOOL dalPluginDirExists =
		[fileManager fileExistsAtPath:dalPluginDestinationPath];
	BOOL dalPluginInstalled =
		[fileManager fileExistsAtPath:dalPluginFileName];
	BOOL dalPluginUpdateNeeded = NO;

	if (dalPluginInstalled) {
		NSDictionary *dalPluginInfoPlist = [NSDictionary
			dictionaryWithContentsOfURL:
				[NSURL fileURLWithPath:
						@"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin/Contents/Info.plist"]];
		NSString *dalPluginVersion = [dalPluginInfoPlist
			valueForKey:@"CFBundleShortVersionString"];
		const char *obsVersion = obs_get_version_string();

		dalPluginUpdateNeeded =
			![dalPluginVersion isEqualToString:@(obsVersion)];
	}

	if (!dalPluginInstalled || dalPluginUpdateNeeded) {
		// TODO: Remove this distinction once OBS is built into an app bundle by cmake by default
		NSString *dalPluginSourcePath;
		NSRunningApplication *app =
			[NSRunningApplication currentApplication];

		if ([app bundleIdentifier] != nil) {
			NSURL *bundleURL = [app bundleURL];
			NSString *pluginPath =
				@"Contents/Resources/data/obs-mac-virtualcam.plugin";

			NSURL *pluginUrl = [bundleURL
				URLByAppendingPathComponent:pluginPath];
			dalPluginSourcePath = [pluginUrl path];
		} else {
			dalPluginSourcePath = [[[[app executableURL]
				URLByAppendingPathComponent:
					@"../data/obs-mac-virtualcam.plugin"]
				path]
				stringByReplacingOccurrencesOfString:@"obs/"
							  withString:@""];
		}

		NSString *createPluginDirCmd =
			(!dalPluginDirExists)
				? [NSString stringWithFormat:
						    @"mkdir -p '%@' && ",
						    dalPluginDestinationPath]
				: @"";
		NSString *deleteOldPluginCmd =
			(dalPluginUpdateNeeded)
				? [NSString stringWithFormat:@"rm -rf '%@' && ",
							     dalPluginFileName]
				: @"";
		NSString *copyPluginCmd =
			[NSString stringWithFormat:@"cp -R '%@' '%@'",
						   dalPluginSourcePath,
						   dalPluginDestinationPath];
		if ([fileManager fileExistsAtPath:dalPluginSourcePath]) {
			NSString *copyCmd = [NSString
				stringWithFormat:
					@"do shell script \"%@%@%@\" with administrator privileges",
					createPluginDirCmd, deleteOldPluginCmd,
					copyPluginCmd];

			NSDictionary *errorDict;
			NSAppleEventDescriptor *returnDescriptor = NULL;
			NSAppleScript *scriptObject =
				[[NSAppleScript alloc] initWithSource:copyCmd];
			returnDescriptor =
				[scriptObject executeAndReturnError:&errorDict];
			if (errorDict != nil) {
				const char *errorMessage = [[errorDict
					objectForKey:@"NSAppleScriptErrorMessage"]
					UTF8String];
				blog(LOG_INFO,
				     "[macOS] VirtualCam DAL Plugin Installation status: %s",
				     errorMessage);
				return false;
			}
		} else {
			blog(LOG_INFO,
			     "[macOS] VirtualCam DAL Plugin not shipped with OBS");
			return false;
		}
	}
	return true;
}

static const char *virtualcam_output_get_name(void *type_data)
{
	(void)type_data;
	return obs_module_text("Plugin_Name");
}

// This is a dummy pointer so we have something to return from virtualcam_output_create
static void *data = &data;

static void *virtualcam_output_create(obs_data_t *settings,
				      obs_output_t *output)
{
	UNUSED_PARAMETER(settings);

	outputRef = output;

	blog(LOG_DEBUG, "output_create");
	sMachServer = [[OBSDALMachServer alloc] init];
	return data;
}

static void virtualcam_output_destroy(void *data)
{
	UNUSED_PARAMETER(data);
	blog(LOG_DEBUG, "output_destroy");
	sMachServer = nil;
}

static bool virtualcam_output_start(void *data)
{
	UNUSED_PARAMETER(data);

	bool hasDalPlugin = check_dal_plugin();

	if (!hasDalPlugin) {
		return false;
	}

	blog(LOG_DEBUG, "output_start");

	[sMachServer run];

	obs_get_video_info(&videoInfo);

	struct video_scale_info conversion = {};
	conversion.format = VIDEO_FORMAT_UYVY;
	conversion.width = videoInfo.output_width;
	conversion.height = videoInfo.output_height;
	obs_output_set_video_conversion(outputRef, &conversion);
	if (!obs_output_begin_data_capture(outputRef, 0)) {
		return false;
	}

	return true;
}

static void virtualcam_output_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(ts);

	blog(LOG_DEBUG, "output_stop");
	obs_output_end_data_capture(outputRef);
	[sMachServer stop];
}

static void virtualcam_output_raw_video(void *data, struct video_data *frame)
{
	UNUSED_PARAMETER(data);

	uint8_t *outData = frame->data[0];
	if (frame->linesize[0] != (videoInfo.output_width * 2)) {
		blog(LOG_ERROR,
		     "unexpected frame->linesize (expected:%d actual:%d)",
		     (videoInfo.output_width * 2), frame->linesize[0]);
	}

	CGFloat width = videoInfo.output_width;
	CGFloat height = videoInfo.output_height;

	[sMachServer sendFrameWithSize:NSMakeSize(width, height)
			     timestamp:frame->timestamp
			  fpsNumerator:videoInfo.fps_num
			fpsDenominator:videoInfo.fps_den
			    frameBytes:outData];
}

struct obs_output_info virtualcam_output_info = {
	.id = "virtualcam_output",
	.flags = OBS_OUTPUT_VIDEO,
	.get_name = virtualcam_output_get_name,
	.create = virtualcam_output_create,
	.destroy = virtualcam_output_destroy,
	.start = virtualcam_output_start,
	.stop = virtualcam_output_stop,
	.raw_video = virtualcam_output_raw_video,
};

bool obs_module_load(void)
{
	blog(LOG_INFO, "version=%s", PLUGIN_VERSION);

	obs_register_output(&virtualcam_output_info);

	obs_data_t *obs_settings = obs_data_create();
	obs_data_set_bool(obs_settings, "vcamEnabled", true);
	obs_apply_private_data(obs_settings);
	obs_data_release(obs_settings);

	return true;
}
