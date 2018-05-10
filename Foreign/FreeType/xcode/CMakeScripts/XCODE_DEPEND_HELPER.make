# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.freetype.Debug:
/Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/Debug/libfreetype.a:
	/bin/rm -f /Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/Debug/libfreetype.a


PostBuild.freetype.Release:
/Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/Release/libfreetype.a:
	/bin/rm -f /Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/Release/libfreetype.a


PostBuild.freetype.MinSizeRel:
/Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/MinSizeRel/libfreetype.a:
	/bin/rm -f /Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/MinSizeRel/libfreetype.a


PostBuild.freetype.RelWithDebInfo:
/Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/RelWithDebInfo/libfreetype.a:
	/bin/rm -f /Users/thiago/repositories/dragons3d/Externals/cocos3d/XLE/Foreign/FreeType/xcode/RelWithDebInfo/libfreetype.a




# For each target create a dummy ruleso the target does not have to exist
