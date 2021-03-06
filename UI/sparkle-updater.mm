#import <Cocoa/Cocoa.h>
#import <Sparkle/Sparkle.h>

static inline bool equali(NSString *a, NSString *b)
{
	return a && b && [a caseInsensitiveCompare:b] == NSOrderedSame;
}

@interface OBSSparkleUpdateDelegate
	: NSObject <SUUpdaterDelegate, SUVersionComparison> {
}
@property (nonatomic) bool updateToUndeployed;
@end

@implementation OBSSparkleUpdateDelegate {
}

@synthesize updateToUndeployed;

- (SUAppcastItem *)bestValidUpdateWithDeltasInAppcast:(SUAppcast *)appcast
					   forUpdater:(SUUpdater *)updater
{
	SUAppcastItem *item = appcast.items.firstObject;
	if (!appcast.items.firstObject)
		return nil;

	SUAppcastItem *app = nil, *mpkg = nil;
	for (SUAppcastItem *item in appcast.items) {
		NSString *deployed = item.propertiesDictionary[@"ce:deployed"];
		if (deployed && !(deployed.boolValue || updateToUndeployed))
			continue;

		NSString *type = item.propertiesDictionary[@"ce:packageType"];
		if (!mpkg && (!type || equali(type, @"mpkg")))
			mpkg = item;
		else if (!app && type && equali(type, @"app"))
			app = item;

		if (app && mpkg)
			break;
	}

	if (app)
		item = app;

	NSBundle *host = updater.hostBundle;
	if (mpkg && (!app || equali(host.bundlePath, @"/Applications/OBS.app")))
		item = mpkg;

	NSMutableDictionary *dict = [NSMutableDictionary
		dictionaryWithDictionary:item.propertiesDictionary];
	NSString *build = [host objectForInfoDictionaryKey:@"CFBundleVersion"];
	NSString *url = dict[@"sparkle:releaseNotesLink"];
	dict[@"sparkle:releaseNotesLink"] =
		[url stringByAppendingFormat:@"#%@", build];

	return [[SUAppcastItem alloc] initWithDictionary:dict];
}

- (SUAppcastItem *)bestValidUpdateInAppcast:(SUAppcast *)appcast
				 forUpdater:(SUUpdater *)updater
{
	SUAppcastItem *selected = [self
		bestValidUpdateWithDeltasInAppcast:appcast
					forUpdater:updater];

	NSBundle *host = updater.hostBundle;
	NSString *build = [host objectForInfoDictionaryKey:@"CFBundleVersion"];
	SUAppcastItem *deltaUpdate = [selected deltaUpdates][build];
	if (deltaUpdate)
		return deltaUpdate;

	return selected;
}

- (NSString *)feedURLStringForUpdater:(SUUpdater *)updater
{
	//URL from Info.plist takes precedence because there may be bundles with
	//differing feed URLs on the system
	NSBundle *bundle = updater.hostBundle;
	return [bundle objectForInfoDictionaryKey:@"SUFeedURL"];
}

- (NSComparisonResult)compareVersion:(NSString *)versionA
			   toVersion:(NSString *)versionB
{
	if (![versionA isEqual:versionB])
		return NSOrderedAscending;
	return NSOrderedSame;
}

- (id<SUVersionComparison>)versionComparatorForUpdater:(SUUpdater *)__unused
	updater
{
	return self;
}

@end

static inline bool bundle_matches(NSBundle *bundle)
{
	if (!bundle.executablePath)
		return false;

	NSRange r = [bundle.executablePath rangeOfString:@"Contents/MacOS/"];
	return [bundle.bundleIdentifier isEqual:@"com.obsproject.obs-studio"] &&
	       r.location != NSNotFound;
}

static inline NSBundle *find_bundle()
{
	NSFileManager *fm = [NSFileManager defaultManager];
	NSString *path = [fm currentDirectoryPath];
	NSString *prev = path;
	do {
		NSBundle *bundle = [NSBundle bundleWithPath:path];
		if (bundle_matches(bundle))
			return bundle;

		prev = path;
		path = [path stringByDeletingLastPathComponent];
	} while (![prev isEqual:path]);
	return nil;
}

static SUUpdater *updater;

static OBSSparkleUpdateDelegate *delegate;

void init_sparkle_updater(bool update_to_undeployed)
{
	updater = [SUUpdater updaterForBundle:find_bundle()];
	delegate = [[OBSSparkleUpdateDelegate alloc] init];
	delegate.updateToUndeployed = update_to_undeployed;
	updater.delegate = delegate;
}

void trigger_sparkle_update()
{
	[updater checkForUpdates:nil];
}
