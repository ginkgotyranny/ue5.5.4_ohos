// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using System.Xml.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/* UnrealPluginLanguage (UPL) is a simple XML-based language for manipulating XML and returning
	 * strings.  It contains an <init> section which is evaluated once per architecture before any
	 * other sections.  The state is maintained and carried forward to the next section evaluated
	 * so the order the sections are executed matters.
	 * 
	 * While UPL is a general system for modifying and quering XML it is specifically used to allow
	 * plug-ins to affect the global configuration of the package that they are a part of. For
	 * example, this allows a plug-in to modify an Android's APK AndroidManfiest.xml file or an
	 * IOS IPA's plist file. UBT will also query a plug-in's UPL xml file for strings to be included
	 * in files that must be common to the package such as some .java files on Android.
	 * 
	 * If you need to see the instructions executed in your plugin context add the following to
	 * enable tracing:
	 * 
	 *	<trace enable="true"/>
	 *	
	 * After this instuction all the nodes actually executed in your context will be written to the
	 * log until you do a <trace enable="false"/>.  You can also get a dump of all the variables in
	 * your context with this command:
	 * 
	 *	<dumpvars/>
	 * 
	 * Bool, Int, and String variable types are supported.  Any attribute may reference a variable
	 * and will be replaced with the string equivalent before evaluation using this syntax:
	 * 
	 *	$B(name) = boolean variable "name"'s value
	 *	$I(name) = integer variable "name"'s value
	 *	$S(name) = string variable "name"'s value
	 *	$E(name) = element variable "name"'s value
	 * 
	 * The following variables are initialized automatically:
	 * 
	 *	$S(Output) = the output returned for evaluating the section (initialized to Input)
	 *	$S(Architecture) = target architecture (armeabi-armv7a, arm64-v8a, x86, x86_64)
	 *	$S(PluginDir) = directory the XML file was loaded from
	 *	$S(EngineDir) = engine directory
	 *	$S(BuildDir) = project's platform appropriate build directory (within the Intermediate folder)
	 *	$S(Configuration) = configuration type (Debug, DebugGame, Development, Test, Shipping)
	 *	$B(Distribution) = true if distribution build
	 *	$B(IsEmbedded) = true if project compiled for embedded use [Android-only]
	 *	$I(EngineMajorVersion) = major version of engine (ex. 4)
	 *	$I(EngineMinorVersion) = minor version of engine (ex. 21)
	 *	$I(EnginePatchVersion) = patch version of engine (ex. 0)
	 *	$S(EngineVersion) = engine version string (ex. "4.21.0")
	 * 
	 * Note: with the exception of the above variables, all are in the context of the plugin to
	 * prevent namespace collision; trying to set a new value to any of the above, with the
	 * exception of Output, will only affect the current context.
	 * 
	 * The following nodes allow manipulation of variables:
	 * 
	 *	<setBool result="" value=""/>
	 *	<setInt result="" value=""/>
	 *	<setString result="" value=""/>
	 *	<setElement result="" value=""/>
	 *	<setElement result="" value="" text=""/>
	 *	<setElement result="" xml=""/>
	 *	
	 * <setElement> with value creates an empty XML element with the tag set to value.
	 * <setElement> with value and text creates an XML element with tag set to value of unparsed text.
	 * <setElement> with xml will parse the XML provided.  Remember to escape any special characters!
	 * 
	 * Variables may also be set from a property in an ini file:
	 * 
	 *	<setBoolFromProperty result="" ini="" section="" property="" default=""/>
	 *	<setBoolFromPropertyContains result="" ini="" section="" property="" default="" contains=""/>
	 *	<setIntFromProperty result="" ini="" section="" property="" default=""/>
	 *	<setIntFromPropertyArrayNum result="" ini="" section="" property=""/>
	 *	<setStringFromProperty result="" ini="" section="" property="" default=""/>
	 *	<setStringFromPropertyArray result="" ini="" section="" property="" index="" default=""/>
     *	
     * Strings may also be set from an environment variable using the <setStringFromEnvVar> node.
	 * The environment variable must be specified as the 'value' attribute and wrapped in a pair
	 * of '%' characters.
     *	
     *	<setStringFromEnvVar result="" value=""/>
	 *	
	 * You can also check if a specific environment variable is defined (again, wrapped in '%' characters):
	 * 
	 *  <setBoolEnvVarDefined result="" value=""/>
	 *  
	 * A general example for using environment variable nodes:
	 * 
	 *  <setBoolEnvVarDefined result="bHasNDK" value="%NDKROOT%"/>
	 *  <setStringFromEnvVar result="NDKPath" value="%NDKROOT%"/>
	 * 
	 * Boolean variables may also be set to the result of applying operators:
	 * 
	 *	<setBoolNot result="" source=""/>
	 *	<setBoolAnd result="" arg1="" arg2=""/>
	 *	<setBoolOr result="" arg1="" arg2=""/>
	 *	<setBoolIsEqual result="" arg1="" arg2=""/>
	 *	<setBoolIsLess result="" arg1="" arg2=""/>
	 *	<setBoolIsLessEqual result="" arg1="" arg2=""/>
	 *	<setBoolIsGreater result="" arg1="" arg2=""/>
	 *	<setBoolIsGreaterEqual result="" arg1="" arg2=""/>
	 *	<setBoolFromPropertyContains result="" ini="" section="" property="" contains=""/>
	 * 
	 * Integer variables may use these arithmetic operations: 
	 * 
	 *	<setIntAdd result="" arg1="" arg2=""/>
	 *	<setIntSubtract result="" arg1="" arg2=""/>
	 *	<setIntMultiply result="" arg1="" arg2=""/>
	 *	<setIntDivide result="" arg1="" arg2=""/>
	 * 
	 * Strings are manipulated with the following:
	 * 
 	 *	<setStringAdd result="" arg1="" arg2=""/>
	 *	<setStringSubstring result="" source="" start="" length=""/>
	 *	<setStringSubstringAfterFind result="" source="" find="" length="" default=""/>
	 *	<setStringReplace result="" source="" find="" with=""/>
	 *	<setStringToLower result="" source=""/>
	 *	<setStringToUpper result="" source=""/>
	 *	
	 * <setStringSubstringAfterFind> is equivalent to calling setStringSubstring with "start" set to
	 * the index of the character immediately following the find string. Passing -1 for length (or
	 * omitting it) will return the entire string to the right of the find string.
	 * 
	 * String length may be retrieved with:
	 * 
	 *	<setIntLength result="" source=""/>
	 * 
	 * The index of a search string may be found in source with:
	 * 
	 *	<setIntFindString result="" source="" find=""/>
	 *	
	 * The following shortcut string comparisons may also be used instead of using <setIntFindString>
	 * and checking the result:
	 * 
	 *	<setBoolStartsWith result="" source="" find=""/>
	 *	<setBoolEndsWith result="" source="" find=""/>
	 *	<setBoolContains result="" source="" find=""/>
	 *	
	 * Messages are written to the log with this node:
	 *
	 *	<log text=""/>
	 * 
	 * Conditional execution uses the following form:
	 * 
	 *	<if condition="">
	 *		<true>
	 *			<!-- executes if boolean variable in condition is true -->
	 *		</true>
	 *		<false>
	 *			<!-- executes if boolean variable in condition is false -->
	 *		</false>
	 *	</if>
	 * 
	 * The <true> and <false> blocks are optional.  The condition must be in a boolean variable.
	 * The boolean operator nodes may be combined to create a final state for more complex
	 * conditions:
	 * 
	 *	<setBoolNot result="notDistribution" source="$B(Distribution)/>
	 *	<setBoolIsEqual result="isX86" arg1="$S(Architecture)" arg2="x86"/>
	 *	<setBoolIsEqual result="isX86_64" arg2="$S(Architecture)" arg2="x86_64/">
	 *	<setBoolOr result="isIntel" arg1="$B(isX86)" arg2="$B(isX86_64)"/>
	 *	<setBoolAnd result="intelAndNotDistribution" arg1="$B(isIntel)" arg2="$B(notDistribution)"/>
	 *	<if condition="intelAndNotDistribution">
	 *		<true>
	 *			<!-- do something for Intel if not a distribution build -->
	 *		</true>
	 *	</if>
	 * 
	 * Note the "isIntel" could also be done like this:
	 * 
	 *	<setStringSubstring result="subarch" source="$S(Architecture)" start="0" length="3"/>
	 *	<setBoolIsEqual result="isIntel" arg1="$S(subarch)" arg2="x86"/>
	 * 
	 * Two shortcut nodes are available for conditional execution:
	 * 
	 *	<isArch arch="armeabi-armv7">
	 *		<!-- do stuff -->
	 *	</isArch>
	 * 
	 * is the equivalent of:
	 * 
	 *	<setBoolIsEqual result="temp" arg1="$S(Architecture)" arg2="armeabi-armv7">
	 *	<if condition="temp">
	 *		<true>
	 *			<!-- do stuff -->
	 *		</true>
	 *	</if>
	 *	
	 * and
	 * 
	 *	<isDistribution>
	 *		<!-- do stuff -->
	 *	</isDistribution>
	 *	
	 * is the equivalent of:
	 * 
	 *	<if condition="Distribution">
	 *		<!-- do stuff -->
	 *	</if>
	 * 
	 * Execution may be stopped with:
	 * 
	 *	<return/>
	 *	 
	 * Loops may be created using these nodes:
	 * 
	 *	<while condition="">
	 *		<!-- do stuff -->
	 *	</while>
	 *	
	 *	<break/>
	 *	<continue/>
	 *	
	 * The <while> body will execute until the condition is false or a <break/> is hit.  The
	 * <continue/> will restart execution of the loop if the condition is still true or exit.
	 * 
	 * Note: <break/> outside a <while> body will act the same as <return/>
	 * 
	 * Here is an example loop which writes 1 to 5 to the log, skipping 3.  Note the update of the
	 * while condition should be done before the continue otherwise it may not exit.
	 * 
	 *	<setInt result="index" value="0"/>
	 *	<setBool result="loopRun" value="true"/>
	 *	<while condition="loopRun">
	 *		<setIntAdd result="index" arg1="$I(index)" arg2="1"/>
	 *		<setBoolIsLess result="loopRun" arg1="$I(index)" arg2="5"/>
	 *		<setBoolIsEqual result="indexIs3" arg1="$I(index)" arg2="3"/>
	 *		<if condition="indexIs3">
	 *			<true>
	 *				<continue/>
	 *			</true>
	 *		</if>
	 *		<log text="$I(index)"/>
	 *	</while>
	 *	
	 * It is possible to use variable replacement in generating the result variable
	 * name as well.  This makes the creation of arrays in loops possible:
	 * 
	 *	<setString result="array_$I(index)" value="element $I(index) in array"/>
	 *	
	 * This may be retrieved using the following (value is treated as the variable
	 * name):
	 * 
	 *	<setStringFrom result="out" value="array_$I(index)"/>
	 *	
	 * For boolean and integer types, you may use <setBoolFrom/> and <setIntFrom/>.
	 * 
	 * Nodes for inserting text into the section are as follows:
	 * 
	 *	<insert> body </insert>
	 *	<insertNewline/>
	 *	<insertValue value=""/>
	 *	<loadLibrary name="" failmsg=""/>
	 *	
	 * The first one will insert either text or nodes into the returned section
	 * string.  Please note you must use escaped characters for:
	 * 
	 *	< = &lt;
	 *	> = &gt;
	 *	& = &amp;
	 * 
	 *	<insertValue value=""/> evaluates variables in value before insertion.  If value contains
	 * double quote ("), you must escape it with &quot;.
	 * 
	 *	<loadLibrary name="" failmsg=""/> is a shortcut to insert a system.LoadLibrary try/catch
	 * block with an optional logged message for failure to load case.
	 * 
	 * You can do a search and replace in the Output with:
	 * 
	 *	<replace find="" with=""/>
	 * 
	 * Note you can also manipulate the actual $S(Output) directly, the above are more efficient:
	 * 
	 *	<setStringAdd result="Input" arg1="$S(Output)" arg2="sample\n"/>
	 *	<setStringReplace result="Input" source="$S(Output)" find=".LAUNCH" with=".INFO"/>
	 *	
	 * XML manipulation uses the following nodes:
	 * 
	 *	<addElement tag="" name=""/>
	 *	<addElements tag=""> body </addElements>
	 *	<removeElement tag=""/>
	 *	<setStringFromTag result="" tag=""/>
	 *	<setStringFromAttribute result="" tag="" name=""/>
	 *	<setStringFromTagText result="" tag=""/>
	 *	<addAttribute tag="" name="" value=""/>
	 *	<removeAttribute tag="" name=""/>
	 *	<loopElements tag=""> instructions </loopElements>
	 *	
	 * The current element is referenced with tag="$".  Element variables are referenced with $varname
	 * since using $E(varname) will be expanded to the string equivalent of the XML.
	 * 
	 * addElement, addElements, and removeElement by default are applied to all matching tags.  An
	 * optional once="true" attribute may be added to only apply to first matching tag.
	 *
	 * <uses-permission>, <uses-feature>, and <uses-library> are updated with:
	 * 
	 *	<addPermission android:name="" .. />
	 *	<addFeature android:name="" .. />
	 *	<addLibrary android:name="" .. />
	 *
	 * Any attributes in the above commands are copied to the element added to the manifest so you
	 * can do the following, for example:
	 * 
	 *	<addFeature android:name="android.hardware.usb.host" android:required="true"/>
	 *	
	 * Finally, these nodes allow copying of files useful for staging jar and so files:
	 * 
	 *	<copyFile src="" dst="" force=""/>
	 *	<copyDir src="" dst="" force=""/>
	 *
	 * If force is false the file(s) are replaced only if length or timestamp don't match.  Default is true.
	 *	
	 * The following should be used as the base for the src and dst paths:
	 * 
	 *	$S(PluginDir) = directory the XML file was loaded from
 	 *	$S(EngineDir) = engine directory
	 *	$S(BuildDir) = project's platform appropriate build directory
	 *	
	 * While it is possible to write outside the APK directory, it is not recommended.
	 * 
	 * If you must remove files (like development-only files from distribution builds) you can
	 * use this node:
	 * 
	 *	<deleteFiles filespec="" recursive=""/>
	 *	
	 * It is restricted to only removing files from the BuildDir. If recursive is true it will search all subfolders.
	 * Here is example usage to remove the Oculus Signature Files (osig) from the assets directory:
	 * 
	 *	<deleteFiles filespec="assets/oculussig_*"/>
	 *	
	 * The following sections are evaluated during the packaging or deploy stages:
	 * 
	 * ** For all platforms **
	 *	<!-- init section is always evaluated once per architecture -->
	 * 	<init> </init>
	 * 
	 * ** Android Specific sections **
	 * 	<!-- optional updates applied to AndroidManifest.xml -->
	 * 	<androidManifestUpdates> </androidManifestUpdates>
	 * 	
	 * 	<!-- optional additions to proguard -->
	 * 	<proguardAdditions>	</proguardAdditions>
	 * 	
	 * 	<!-- optional AAR imports additions -->
	 * 	<AARImports> </AARImports>
	 * 	
	 * 	<!-- optional base build.gradle additions -->
	 * 	<baseBuildGradleAdditions>  </baseBuildGradleAdditions>
	 *
	 *	<!-- optional base build.gradle buildscript additions -->
	 *	<buildscriptGradleAdditions>  </buildscriptGradleAdditions>
	 *	
	 * 	<!-- optional app build.gradle additions -->
	 * 	<buildGradleAdditions>  </buildGradleAdditions>
	 * 	
	 * 	<!-- optional additions to generated build.xml before ${sdk.dir}/tools/ant/build.xml import -->
	 * 	<buildXmlPropertyAdditions> </buildXmlPropertyAdditions>
	 *
	 * 	<!-- optional files or directories to copy or delete from Intermediate/Android/APK before ndk-build -->
	 * 	<prebuildCopies> </prebuildCopies>
	 * 	
	 * 	<!-- optional files or directories to copy or delete from Intermediate/Android/APK after ndk-build -->
	 * 	<resourceCopies> </resourceCopies>
	 * 	
	 * 	<!-- optional files or directories to copy or delete from Intermediate/Android/APK before Gradle -->
	 * 	<gradleCopies> </gradleCopies>
	 * 	
	 * 	<!-- optional properties to add to gradle.properties -->
	 * 	<gradleProperties> </gradleProperties>
	 *
 	 * 	<!-- optional parameters to add to Gradle commandline (prefix with a space or will run into previous parameter(s)) -->
	 * 	<gradleParameters> </gradleParameters>
	 *
	 *  <!-- optional minimum SDK API level required -->
	 *  <minimumSDKAPI> </minimumSDKAPI>
	 * 	
	 * 	<!-- optional additions to the GameActivity imports in GameActivity.java -->
	 * 	<gameActivityImportAdditions> </gameActivityImportAdditions>
	 *
	 * 	<!-- optional additions to the GameActivity after imports in GameActivity.java -->
	 *  <gameActivityPostImportAdditions> </gameActivityPostImportAdditions>
	 *  
	 * 	<!-- optional additions to the GameActivity class implements in GameActivity.java (end each line with a comma) -->
	 * 	<gameActivityImplementsAdditions> </gameActivityImplementsAdditions>
	 *
	 * 	<!-- optional additions to the GameActivity class body in GameActivity.java -->
	 * 	<gameActivityClassAdditions> </gameActivityOnClassAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onCreate metadata reading in GameActivity.java -->
	 * 	<gameActivityReadMetadata> </gameActivityReadMetadata>
	 * 
	 * 	<!-- optional additions to the very earliest point in GameActivity onCreate in GameActivity.java -->
	 *	<gameActivityOnCreateBeginningAdditions> </gameActivityOnCreateBeginningAdditions>
	 *	
	 *	<!-- optional additions to GameActivity onCreate in GameActivity.java -->
	 *	<gameActivityOnCreateAdditions> </gameActivityOnCreateAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onDestroy in GameActivity.java -->
	 * 	<gameActivityOnDestroyAdditions> </gameActivityOnDestroyAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onConfigurationChanged in GameActivity.java -->
	 * 	<gameActivityonConfigurationChangedAdditions> </gameActivityonConfigurationChangedAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onStart in GameActivity.java -->
	 * 	<gameActivityOnStartAdditions> </gameActivityOnStartAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onStop in GameActivity.java -->
	 * 	<gameActivityOnStopAdditions> </gameActivityOnStopAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onPause in GameActivity.java -->
	 * 	<gameActivityOnPauseAdditions> </gameActivityOnPauseAdditions>
	 * 	
	 * 	<!-- optional additions to GameActivity onResume in GameActivity.java -->
	 *	<gameActivityOnResumeAdditions>	</gameActivityOnResumeAdditions>
	 *	
	 * 	<!-- optional additions to GameActivity onNewIntent in GameActivity.java -->
	 *	<gameActivityOnNewIntentAdditions> </gameActivityOnNewIntentAdditions>
	 *	
	 * 	<!-- optional additions to GameActivity onActivityResult in GameActivity.java -->
	 * 	<gameActivityOnActivityResultAdditions>	</gameActivityOnActivityResultAdditions>
	 * 	
	 * 	<!-- optional libraries to load in GameActivity.java before libUnreal.so -->
	 * 	<soLoadLibrary>	</soLoadLibrary>
	 * 	
	 * 
	 * Here is the complete list of supported nodes:
	 * 
	 * <isArch arch="">
	 * <isDistribution>
	 * <if> => <true> / <false>
	 * <while condition="">
	 * <return/>
	 * <break/>
	 * <continue/>
	 * <log text=""/>
	 * <insert> </insert>
	 * <insertValue value=""/>
	 * <replace find="" with=""/>
	 * <copyFile src="" dst=""/>
	 * <copyDir src="" dst=""/>
	 * <loadLibrary name="" failmsg=""/>
	 * <setBool result="" value=""/>
	 * <setBoolEnvVarDefined result="" value=""/>
	 * <setBoolFrom result="" value=""/>
	 * <setBoolFromProperty result="" ini="" section="" property="" default=""/>
	 * <setBoolFromPropertyContains result="" ini="" section="" property="" contains=""/>
	 * <setBoolNot result="" source=""/>
	 * <setBoolAnd result="" arg1="" arg2=""/>
	 * <setBoolOr result="" arg1="" arg2=""/>
	 * <setBoolIsEqual result="" arg1="" arg2=""/>
	 * <setBoolIsLess result="" arg1="" arg2=""/>
	 * <setBoolIsLessEqual result="" arg1="" arg2=""/>
	 * <setBoolIsGreater result="" arg1="" arg2=""/>
	 * <setBoolIsGreaterEqual result="" arg1="" arg2=""/>
	 * <setInt result="" value=""/>
	 * <setIntFrom result="" value=""/>
	 * <setIntFromProperty result="" ini="" section="" property="" default=""/>
	 * <setIntFromPropertyArrayNum result="" ini="" section="" property=""/>
	 * <setIntAdd result="" arg1="" arg2=""/>
	 * <setIntSubtract result="" arg1="" arg2=""/>
	 * <setIntMultiply result="" arg1="" arg2=""/>
	 * <setIntDivide result="" arg1="" arg2=""/>
	 * <setIntLength result="" source=""/>
	 * <setIntFindString result="" source="" find=""/>
	 * <setString result="" value=""/>
	 * <setStringFrom result="" value=""/>
     * <setStringFromEnvVar result="" value=""/>
	 * <setStringFromProperty result="" ini="" section="" property="" default=""/>
	 * <setStringFromPropertyArray result="" ini="" section="" property="" index="" default=""/>
	 * <setStringAdd result="" arg1="" arg2=""/>
	 * <setStringSubstring result="" source="" index="" length=""/>
	 * <setStringSubstringAfterFind result="" source="" find="" length="" default=""/>
	 * <setStringReplace result="" source="" find="" with=""/>
	 * <setStringToLower result="" source=""/>
	 * <setStringToUpper result="" source=""/>
	 * 
	 */

	// Helper struct to initialize aliased namespaces
	struct AliasedXMLNamespace
	{
		public string Alias { get; set; }
		public string Url { get; set; }

		public override string ToString()
		{
			return String.Format("xmlns:{0}=\"{1}\"", Alias, Url);
		}
	}

	class UnrealPluginLanguage
	{
		/** The merged XML program to run */
		private XDocument XDoc;

		/** XML namespace */
		private XNamespace XMLDefaultNameSpace;
		private Dictionary<string, XNamespace> XMLNameSpaceAliases = new Dictionary<string, XNamespace>();
		private string XMLRootDefinition;

		private UnrealTargetPlatform TargetPlatform;

		/** Trace flag to enable debugging */
		private static bool bGlobalTrace = false;
		private static bool bGlobalTraceFilename = false;

		/** Project file reference */
		private FileReference? ProjectFile;
		private ConfigHierarchy EngineIni;
		private ConfigHierarchy GameIni;

		private static XDocument XMLDummy = XDocument.Parse("<manifest></manifest>");
		
		private struct UriInSkill
		{
			public string scheme;
			public string host;
			public string port;
			public string path;
			public string pathStartWith;
			public string pathRegex;
			public string type;
			public string utd;
			public int maxFileSupported;
			public string linkFeature;
		}
    	
		private class AbilitySkill
		{
			public HashSet<string> Actions;
			public HashSet<string> Entities;
			public List<UriInSkill> Uris;
			public HashSet<string> Permissions;
			public bool DomainVerify;
    
			public AbilitySkill() 
			{
				Actions = new HashSet<string>();
				Entities = new HashSet<string>();
				Uris = new List<UriInSkill>();
				Permissions = new HashSet<string>();
				DomainVerify = false;
			}
    
			public AbilitySkill(HashSet<string> Actions, HashSet<string> Entities, List<UriInSkill> Uris, HashSet<string> Permissions, bool DomainVerify)
			{
				this.Actions = Actions;
				this.Entities = Entities;
				this.Uris = Uris;
				this.Permissions = Permissions;
				this.DomainVerify = DomainVerify;
			}
		}
		
		private struct MetaData
		{
			public string name;
			public string value;
			public string resource;
		}

		private class PermissionComparer : IEqualityComparer<RequestPermission>
		{
			public bool Equals( RequestPermission? rp1,  RequestPermission? rp2)
			{
				return rp1?.name == rp2?.name;
			}
 
			public int GetHashCode( RequestPermission rp)
			{
				return rp.name.GetHashCode();
			}
		}

		private class RequestPermission
		{
			public string name;
			public string reason;
			public string[] usedScene_abilities;
			public string usedScene_when;

			public RequestPermission() {
				this.name = "";
				this.reason = "$string:EntryAbility_desc";
				this.usedScene_abilities = new[] { "EntryAbility" };
				this.usedScene_when = "always";
			}

			public RequestPermission(string name) {
				this.name = name;
				this.reason = "$string:EntryAbility_desc";
				this.usedScene_abilities = new[] { "EntryAbility" };
				this.usedScene_when = "always";
			}
		}
		
		// reason映射，用于在string.json中添加对应的定义
		public struct ReasonMapping
		{
			public Dictionary<string, string> ReasonDict;
			public int Index;

			public ReasonMapping()
			{
				ReasonDict = new Dictionary<string, string>();
				Index = 1;
			}
		}

		private class UPLContext
		{
			public string Filename;

			/** Variable state */
			public Dictionary<string, bool> BoolVariables;
			public Dictionary<string, int> IntVariables;
			public Dictionary<string, string> StringVariables;
			public Dictionary<string, XElement> ElementVariables;
			
			/** HPL数据存储 */
			public Dictionary<string, HashSet<string>> StringValueUpdates;	
			public Dictionary<string, Dictionary<string, string>> StringPairUpdates;
			public List<UriInSkill> UriList;
			public HashSet<MetaData> MetaDataSet;
			public List<AbilitySkill> SkillList;
			public HashSet<RequestPermission> AddPermissions;
			public ReasonMapping ReasonMap;

			/** Local context trace */
			public bool bTrace;
			public bool bTraceFilename;

			public UPLContext(string inFilename, string Architecture, string PluginDir)
			{
				Filename = inFilename;

				BoolVariables = new Dictionary<string, bool>();
				IntVariables = new Dictionary<string, int>();
				StringVariables = new Dictionary<string, string>();
				ElementVariables = new Dictionary<string, XElement>();
				
				// HPL数据容器初始化
				StringValueUpdates = new Dictionary<string, HashSet<string>>();
				StringPairUpdates = new Dictionary<string, Dictionary<string, string>>();
				MetaDataSet = new HashSet<MetaData>();
				UriList = new List<UriInSkill>();
				SkillList = new List<AbilitySkill>();
				ReasonMap = new ReasonMapping();
				AddPermissions = new HashSet<RequestPermission>(new PermissionComparer())
				{
					new RequestPermission("ohos.permission.INTERNET"), 
					new RequestPermission("ohos.permission.SET_NETWORK_INFO"), 
					new RequestPermission("ohos.permission.GET_NETWORK_INFO"),
					new RequestPermission("ohos.permission.VIBRATE"),
					new RequestPermission("ohos.permission.APPROXIMATELY_LOCATION"),
					new RequestPermission("ohos.permission.LOCATION"),
					new RequestPermission("ohos.permission.LOCATION_IN_BACKGROUND")
				};
				
				StringValueUpdates.Add("OhpmrcRegistry", new HashSet<string>()
				{
					"https://ohpm.openharmony.cn/ohpm/"
				});

				if (PluginDir == null || String.IsNullOrEmpty(PluginDir))
				{
					PluginDir = ".";
				}
				StringVariables["PluginDir"] = PluginDir.Replace("\\", "/");
				StringVariables["AbsPluginDir"] = Path.GetFullPath(PluginDir).Replace("\\", "/");

				StringVariables["Architecture"] = Architecture;

				bTrace = false;
				bTraceFilename = false;
			}
		}

		/// <summary>
		/// 处理鸿蒙特殊HPL节点
		/// </summary>
		private abstract class OpenHarmonyPluginLanguageNodeProcessor
		{
			public UnrealPluginLanguage UPL;
			public Stack<XElement> ExecutionStack = new Stack<XElement>();

			public OpenHarmonyPluginLanguageNodeProcessor(UnrealPluginLanguage InUPL)
			{
				UPL = InUPL;
			}
			
			public void ProcessNode(XElement ParentNode)
			{
				foreach (XElement Instruction in ParentNode.Elements().Reverse())
				{
					ExecutionStack.Push(Instruction);
				}

				if (ExecutionStack.Count == 0)
				{
					return;
				}

				Stack<UPLContext> ContextStack = new Stack<UPLContext>();
				UPLContext CurrentContext = UPL.GlobalContext;
				UPL.GlobalContext.UriList = new List<UriInSkill>();

				// update Output in global context
				UPL.GlobalContext.StringVariables["Output"] = "";

				// run the instructions
				while (ExecutionStack.Count > 0)
				{
					XElement Node = ExecutionStack.Pop();
					if (bGlobalTrace || CurrentContext.bTrace)
					{
						UPL.Logger.LogInformation("Execute: '{Node}'",
							UPL.TraceNodeString(CurrentContext, Node, bGlobalTraceFilename || CurrentContext.bTraceFilename));
					}

					SafeProcessNodeCase(CurrentContext, Node, ParentNode.Name.ToString());
				}
			}

			// public abstract void ProcessNodeCase(UPLContext CurrentContext, XElement Node);
			
			/// <summary>
			/// 增加父节点检测，防止子节点写错位置
			/// </summary>
			/// <param name="CurrentContext"></param>
			/// <param name="Node"></param>
			/// <param name="ParentNodeName"></param>
			public abstract void SafeProcessNodeCase(UPLContext CurrentContext, XElement Node, string ParentNodeName);
		}

		/// <summary>
		/// HPL一级子节点处理
		/// </summary>
		private class FirstLevelSubNodeProcessor : OpenHarmonyPluginLanguageNodeProcessor
		{
			public FirstLevelSubNodeProcessor(UnrealPluginLanguage InUPL) : base(InUPL) { }
			public override void SafeProcessNodeCase(UPLContext CurrentContext, XElement Node, string ParentNodeName)
			{
				UPL.Logger.LogInformation("Log UPL: safeProcessNode '{0}'", Node.Name.ToString());
				switch (Node.Name.ToString())
				{
					case "trace":
						{
							string? TraceFilename = UPL.GetAttribute(CurrentContext, Node, "filename");
							if (TraceFilename != null)
							{
								CurrentContext.bTraceFilename = UPL.StringToBool(TraceFilename);
							}
						}
						break;

					case "UsedSceneAbilities":
						{
							if (ParentNodeName != "addCustomRequestPermission")
							{
								UPL.Logger.LogError("HPL Node 'UsedSceneAbilities' can't be used to '{0}'", ParentNodeName);
								break;
							}
							
							string[] Abilities = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							UPL.GlobalContext.StringValueUpdates["UsedSceneAbilities"] = new HashSet<string>();
							foreach (var ability in Abilities) 
							{
								UPL.GlobalContext.StringValueUpdates["UsedSceneAbilities"].Add(ability.Trim());
							}
						}
						break;

					case "SkillActions":
						{
							if (ParentNodeName != "addSkill")
							{
								UPL.Logger.LogError("HPL Node 'SkillActions' can't be used to '{0}'", ParentNodeName);
								break;
							}
							
							string[] Actions = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							UPL.GlobalContext.StringValueUpdates["SkillActions"] = new HashSet<string>();
							foreach (var action in Actions)
							{
								UPL.GlobalContext.StringValueUpdates["SkillActions"].Add(action.Trim());
							}
						}
						break;

					case "SkillEntities":
						{
							if (ParentNodeName != "addSkill")
							{
								UPL.Logger.LogError("HPL Node 'SkillEntities' can't be used to '{0}'", ParentNodeName);
								break;
							}
							
							string[] Entities = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							UPL.GlobalContext.StringValueUpdates["SkillEntities"] = new HashSet<string>();
							foreach (var entity in Entities)
							{
								UPL.GlobalContext.StringValueUpdates["SkillEntities"].Add(entity.Trim());
							}
						}
						break;

					case "SkillUri":
						{
							if (ParentNodeName != "addSkill")
							{
								UPL.Logger.LogError("HPL Node 'SkillEntities' can't be used to '{0}'", ParentNodeName);
								break;
							}
							
							string? Scheme = UPL.GetAttribute(CurrentContext, Node, "scheme") ?? String.Empty;
							string? Host = UPL.GetAttribute(CurrentContext, Node, "host") ?? String.Empty;
							string? Port = UPL.GetAttribute(CurrentContext, Node, "port") ?? String.Empty;
							string? Path = UPL.GetAttribute(CurrentContext, Node, "path") ?? String.Empty;
							string PathStartWith = UPL.GetAttribute(CurrentContext, Node, "pathStartWith") ?? String.Empty;
							string PathRegex = UPL.GetAttribute(CurrentContext, Node, "pathRegex") ?? String.Empty;
							string Type = UPL.GetAttribute(CurrentContext, Node, "type") ?? String.Empty;
							string Utd = UPL.GetAttribute(CurrentContext, Node, "utd") ?? String.Empty;
							if (!int.TryParse(UPL.GetAttribute(CurrentContext, Node, "maxFileSupported"), out var MaxFileSupported)) {
								MaxFileSupported = 0;
							}
							string LinkFeature = UPL.GetAttribute(CurrentContext, Node, "linkFeature") ?? String.Empty;

							UriInSkill tempUri = new UriInSkill();
							tempUri.scheme = Scheme;
							tempUri.host = Host;
							tempUri.port = Port;
							tempUri.path = Path;
							tempUri.pathStartWith = PathStartWith;
							tempUri.pathRegex = PathRegex;
							tempUri.type = Type;
							tempUri.utd = Utd;
							tempUri.maxFileSupported = MaxFileSupported;
							tempUri.linkFeature = LinkFeature;
							
							UPL.GlobalContext.UriList.Add(tempUri);
						}
						break;

					case "SkillPermissions":
						{
							if (ParentNodeName != "addSkill")
							{
								UPL.Logger.LogError("HPL Node 'SkillPermissions' can't be used to '{0}'", ParentNodeName);
								break;
							}
							
							string[] Entities = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							UPL.GlobalContext.StringValueUpdates["SkillPermissions"] = new HashSet<string>();
							foreach (var entity in Entities)
							{
								UPL.GlobalContext.StringValueUpdates["SkillPermissions"].Add(entity.Trim());
							}
						}
						break;
					
					default:
						{
							UPL.Logger.LogInformation("Unknown Command {Name} in '{0}'", Node.Name, ParentNodeName);
						}
						break;
				}
			}
		}

		private UPLContext GlobalContext;
		private Dictionary<string, UPLContext> Contexts;
		private int ContextIndex;
		private string? LastError;
		private ILogger Logger;

		// First entry in InXMLNameSpaceAliases will be used as default namespace
		public UnrealPluginLanguage(FileReference? InProjectFile, List<string> InXMLFiles, List<string> InArchitectures, List<AliasedXMLNamespace>? InXMLNameSpaceAliases, UnrealTargetPlatform InTargetPlatform, ILogger InLogger)
		{
			ProjectFile = InProjectFile;
			Logger = InLogger;

			LastError = null;

			Contexts = new Dictionary<string, UPLContext>();
			GlobalContext = new UPLContext("", "", "");
			ContextIndex = 0;
			
			if (InXMLNameSpaceAliases != null && InXMLNameSpaceAliases.Count > 0)
			{
				XMLDefaultNameSpace = InXMLNameSpaceAliases[0].Url;

				StringBuilder RootNamespaces = new StringBuilder();
				foreach (AliasedXMLNamespace AliasToNamespace in InXMLNameSpaceAliases)
				{
					XMLNameSpaceAliases[AliasToNamespace.Alias] = AliasToNamespace.Url;
					RootNamespaces.AppendFormat("xmlns:{0}=\"{1}\" ", AliasToNamespace.Alias, AliasToNamespace.Url);
				}

				XMLRootDefinition = String.Join(" ", RootNamespaces);
			}
			else
			{
				XMLDefaultNameSpace = "";
				XMLRootDefinition = "";
			}
			TargetPlatform = InTargetPlatform;
			
			EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			GameIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);

			string PathPrefix = Path.GetFileName(Directory.GetCurrentDirectory()).Equals("Source") ? ".." : "Engine";

			XDoc = XDocument.Parse("<root " + XMLRootDefinition + "></root>");
			foreach (string Basename in InXMLFiles)
			{
				string Filename = Path.Combine(PathPrefix, Basename.Replace("\\", "/"));
				Logger.LogInformation("UPL: {FileName}", Filename);
				if (File.Exists(Filename))
				{
					string PluginDir = Path.GetDirectoryName(Filename)!;
					try
					{
						XDocument MergeDoc = XDocument.Load(Filename, LoadOptions.SetLineInfo);
						MergeXML(MergeDoc, Filename, PluginDir, InArchitectures);
					}
					catch (Exception e)
					{
						LastError = String.Format("Unreal Plugin file {0} parsing failed! {1}", Filename, e);
						Logger.LogError("\n{LastError}", LastError);
					}
				}
				else
				{
					LastError = String.Format("Unreal Plugin file {0} missing!", Filename);
					Logger.LogError("\n{LastError}", LastError);
					Logger.LogInformation("\nCWD: {Cwd}", Directory.GetCurrentDirectory());
				}
			}
		}

		public string? GetLastError() { return LastError; }

		public bool GetTrace() { return bGlobalTrace; }
		public void SetTrace() { bGlobalTrace = true; }
		public void ClearTrace() { bGlobalTrace = false; }
		public bool GetTraceFilename() { return bGlobalTraceFilename; }
		public void SetTraceFilename() { bGlobalTraceFilename = true; }
		public void ClearTraceFilename() { bGlobalTraceFilename = false; }

		public string GetUPLHash()
		{
			return ContentHash.MD5(XDoc.ToString()).ToString();
		}

		public bool MergeXML(XDocument MergeDoc, string Filename, string PluginDir, List<string> Architectures)
		{
			if (MergeDoc == null)
			{
				return false;
			}

			// create a context for each architecture
			ContextIndex++;
			foreach (string Architecture in Architectures)
			{
				UPLContext Context = new UPLContext(Filename, Architecture, PluginDir);
				Contexts[Architecture + "_" + ContextIndex] = Context;
			}

			// line numbers are lost in the merge so add them as an attribute for later tracing of warnings/errors
			foreach (XElement Element in MergeDoc.Root!.Descendants())
			{
				if (((IXmlLineInfo)Element).HasLineInfo())
				{
					Element.Add(new XAttribute("__line", ((IXmlLineInfo)Element).LineNumber));
				}
			}

			// merge in the nodes
			foreach (XElement Element in MergeDoc.Root!.Elements())
			{
				XElement? Parent = XDoc.Root!.Element(Element.Name);
				if (Parent != null)
				{
					XElement Entry = new XElement("Context", new XAttribute("index", ContextIndex.ToString()));
					Entry.Add(Element.Elements());
					Parent.Add(Entry);
				}
				else
				{
					XElement Entry = new XElement("Context", new XAttribute("index", ContextIndex.ToString()));
					Entry.Add(Element.Elements());
					XElement Base = new XElement(Element.Name);
					Base.Add(Entry);
					XDoc.Root.Add(Base);
				}
			}

			return true;
		}

		public void SaveXML(string Filename)
		{
			XDoc?.Save(Filename);
		}

		private string DumpContext(UPLContext Context)
		{
			StringBuilder Text = new StringBuilder();
			foreach (KeyValuePair<string, bool> Variable in Context.BoolVariables)
			{
				Text.AppendLine(String.Format("\tbool {0} = {1}", Variable.Key, Variable.Value.ToString().ToLower()));
			}
			foreach (KeyValuePair<string, int> Variable in Context.IntVariables)
			{
				Text.AppendLine(String.Format("\tint {0} = {1}", Variable.Key, Variable.Value));
			}
			foreach (KeyValuePair<string, string> Variable in Context.StringVariables)
			{
				Text.AppendLine(String.Format("\tstring {0} = {1}", Variable.Key, Variable.Value));
			}
			foreach (KeyValuePair<string, XElement> Variable in Context.ElementVariables)
			{
				Text.AppendLine(String.Format("\telement {0} = {1}", Variable.Key, Variable.Value));
			}
			return Text.ToString();
		}

		public string DumpVariables()
		{
			string Result = "Global Context:\n" + DumpContext(GlobalContext);
			foreach (KeyValuePair<string, UPLContext> Context in Contexts)
			{
				Result += "Context " + Context.Key + ": " + Context.Value.StringVariables["PluginDir"] + "\n" + DumpContext(Context.Value);
			}
			return Result;
		}

		private bool GetCondition(UPLContext Context, XElement Node, string? Condition, out bool Result)
		{
			Result = false;

			if (Condition == null)
			{
				return false;
			}

			if (!Context.BoolVariables.TryGetValue(Condition, out Result))
			{
				if (!GlobalContext.BoolVariables.TryGetValue(Condition, out Result))
				{
					Logger.LogWarning("\nMissing condition '{Condition}' in '{Node}' (skipping instruction)", Condition, TraceNodeString(Context, Node));
					return false;
				}
			}
			return true;
		}

		private string ExpandVariables(UPLContext Context, string InputString)
		{
			string Result = InputString;
			for (int Idx = Result.IndexOf("$B("); Idx != -1; Idx = Result.IndexOf("$B(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 3);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 3, EndIdx - (Idx + 3));

				// Find the value for it, either from the dictionary or the environment block
				bool Value;
				if (!Context.BoolVariables.TryGetValue(Name, out Value))
				{
					if (!GlobalContext.BoolVariables.TryGetValue(Name, out Value))
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value.ToString().ToLower() + Result.Substring(EndIdx + 1);
			}
			for (int Idx = Result.IndexOf("$I("); Idx != -1; Idx = Result.IndexOf("$I(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 3);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 3, EndIdx - (Idx + 3));

				// Find the value for it, either from the dictionary or the environment block
				int Value;
				if (!Context.IntVariables.TryGetValue(Name, out Value))
				{
					if (!GlobalContext.IntVariables.TryGetValue(Name, out Value))
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value.ToString() + Result.Substring(EndIdx + 1);
			}
			for (int Idx = Result.IndexOf("$S("); Idx != -1; Idx = Result.IndexOf("$S(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 3);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 3, EndIdx - (Idx + 3));

				// Find the value for it, either from the dictionary or the environment block
				string? Value;
				if (!Context.StringVariables.TryGetValue(Name, out Value))
				{
					if (!GlobalContext.StringVariables.TryGetValue(Name, out Value))
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value + Result.Substring(EndIdx + 1);
			}
			for (int Idx = Result.IndexOf("$E("); Idx != -1; Idx = Result.IndexOf("$E(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 3);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 3, EndIdx - (Idx + 3));

				// Find the value for it, either from the dictionary or the environment block
				XElement? Value;
				if (!Context.ElementVariables.TryGetValue(Name, out Value))
				{
					if (!GlobalContext.ElementVariables.TryGetValue(Name, out Value))
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value + Result.Substring(EndIdx + 1);
			}
			return Result;
		}

		private string TraceNodeString(UPLContext Context, XElement Node, bool ShowFilename = true)
		{
			XAttribute? LineAttrib = null;
			string Result = Node.Name.ToString();
			foreach (XAttribute Attrib in Node.Attributes())
			{
				string AttribStr = Attrib.ToString();
				if (AttribStr.StartsWith("__line="))
				{
					LineAttrib = Attrib;
				}
				else
				{
					Result += " " + AttribStr;
				}
			}

			if (ShowFilename)
			{
				Result += ", File: " + Context.Filename;
			}
			if (LineAttrib != null)
			{
				Result += ", Line: " + LineAttrib.Value;
			}

			return Result;
		}

		private bool StringToBool(string? Input)
		{
			if (Input == null)
			{
				return false;
			}
			Input = Input.ToLower();
			return !(Input.Equals("0") || Input.Equals("false") || Input.Equals("off") || Input.Equals("no"));
		}

		private int StringToInt(UPLContext Context, XElement Node, string? Input)
		{
			int Result = 0;
			if (!Int32.TryParse(Input, out Result))
			{
				Logger.LogWarning("\nInvalid integer '{Input}' in '{Node}' (defaulting to 0)", Input, TraceNodeString(Context, Node));
			}
			return Result;
		}

		[return: NotNullIfNotNull("Fallback")]
		private string? GetAttribute(UPLContext Context, XElement Node, string AttributeName, bool bExpand = true, bool bRequired = true, string? Fallback = null)
		{
			XAttribute? Attribute = Node.Attribute(AttributeName);
			if (Attribute == null)
			{
				if (bRequired)
				{
					Logger.LogWarning("\nMissing attribute '{Attribute}' in '{Node}' (skipping instruction)", AttributeName, TraceNodeString(Context, Node));
				}
				return Fallback;
			}
			string Result = Attribute.Value;
			return bExpand ? ExpandVariables(Context, Result) : Result;
		}

		[return: NotNullIfNotNull("Fallback")]
		private string? GetAttributeWithNamespace(UPLContext Context, XElement Node, XNamespace Namespace, string AttributeName, bool bExpand = true, bool bRequired = true, string? Fallback = null)
		{
			XAttribute? Attribute = Node.Attribute(Namespace + AttributeName);
			if (Attribute == null)
			{
				if (bRequired)
				{
					Logger.LogWarning("\nMissing attribute '{Attribute}' in '{Node}' (skipping instruction)", AttributeName, TraceNodeString(Context, Node));
				}
				return Fallback;
			}
			string Result = Attribute.Value;
			return bExpand ? ExpandVariables(Context, Result) : Result;
		}

		// 重命名是因为变量名与ConfigCache类名重了，导致本文件其他地方调用类ConfigCache编译报错
		private static Dictionary<string, ConfigCacheIni_UPL>? ConfigCacheDict = null;

		private ConfigCacheIni_UPL GetConfigCacheIni_UPL(string baseIniName)
		{
			ConfigCacheDict ??= new Dictionary<string, ConfigCacheIni_UPL>();
			ConfigCacheIni_UPL? config = null;
			if (!ConfigCacheDict.TryGetValue(baseIniName, out config))
			{
				// note: use our own ConfigCacheIni since EngineConfiguration.cs only parses RequiredSections!
				config = ConfigCacheIni_UPL.CreateConfigCacheIni_UPL(TargetPlatform, baseIniName, DirectoryReference.FromFile(ProjectFile), Logger);
				ConfigCacheDict.Add(baseIniName, config);
			}
			return config;
		}

		private static bool FilesAreDifferent(string SourceFilename, string DestFilename, ILogger Logger)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				Logger.LogInformation("File {SourceFilename} does not exist", SourceFilename);
				return false;
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return true;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return true;
			}

			// validate timestamps
			TimeSpan Diff = DestInfo.LastWriteTimeUtc - SourceInfo.LastWriteTimeUtc;
			if (Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				return true;
			}

			// could check actual bytes just to be sure, but good enough
			return false;
		}

		private static void CopyFileDirectory(string SourceDir, string DestDir, ILogger Logger, bool bForce = false)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));

				if (bForce || FilesAreDifferent(Filename, DestFilename, Logger))
				{
					if (File.Exists(DestFilename))
					{
						File.SetAttributes(DestFilename, FileAttributes.Normal);
						File.Delete(DestFilename);
					}

					// make the subdirectory if needed
					string DestSubdir = Path.GetDirectoryName(DestFilename)!;
					if (!Directory.Exists(DestSubdir))
					{
						Directory.CreateDirectory(DestSubdir);
					}

					File.Copy(Filename, DestFilename);

					// remove any read only flags and keep timestamp
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes &= ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));
				}
			}
		}

		private static void DeleteFiles(string Filespec, bool Recursive, ILogger Logger)
		{
			string BaseDir = Path.GetDirectoryName(Filespec)!;
			string Mask = Path.GetFileName(Filespec);

			if (!Directory.Exists(BaseDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(BaseDir, Mask, Recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly);
			foreach (string Filename in Files)
			{
				File.SetAttributes(Filename, FileAttributes.Normal);
				File.Delete(Filename);
				Logger.LogInformation("\nDeleted file {Filename}", Filename);
			}
		}

		XNamespace? TrimNamespaceAliasFromName(ref string Name)
		{
			int Index = Name.IndexOf(":");
			if (Index >= 0)
			{
				XNamespace? Namespace;
				string NamespaceAlias = Name.Substring(0, Index);
				if (!XMLNameSpaceAliases.TryGetValue(NamespaceAlias, out Namespace))
				{
					Namespace = XMLDefaultNameSpace;
				}
				Name = Name.Substring(Index + 1);
				return Namespace;
			}
			else
			{
				return null;
			}
		}
		private void AddAttribute(XElement Element, string Name, string Value)
		{
			XNamespace? XMLNameSpace = TrimNamespaceAliasFromName(ref Name);
			XAttribute? Attribute = Element.Attribute(XMLNameSpace != null? XMLNameSpace + Name : Name);

			if (Attribute != null)
			{
				Attribute.SetValue(Value);
			}
			else
			{
				Element.Add(new XAttribute(XMLNameSpace != null ? XMLNameSpace + Name : Name, Value));
			}
		}

		private void RemoveAttribute(XElement Element, string Name)
		{
			XNamespace? XMLNameSpace = TrimNamespaceAliasFromName(ref Name);
			XAttribute? Attribute = Element.Attribute(XMLNameSpace != null ? XMLNameSpace + Name : Name);

			Attribute?.Remove();
		}

		private void AddElements(XElement Target, XElement Source)
		{
			if (Source.HasElements)
			{
				foreach (XElement Index in Source.Elements())
				{
					//					if (Target.Element(Index.Name) == null)
					{
						Target.Add(Index);
					}
				}
			}
		}

		public string ProcessPluginNode(string Architecture, string NodeName, string Input)
		{
			return ProcessPluginNode(Architecture, NodeName, Input, ref XMLDummy);
		}

		public string ProcessPluginNode(string Architecture, string NodeName, string Input, ref XDocument XMLWork)
		{
			// add all instructions to execution list
			Stack<XElement> ExecutionStack = new Stack<XElement>();
			XElement? StartNode = XDoc.Root!.Element(NodeName);
			if (StartNode != null)
			{
				foreach (XElement Instruction in StartNode.Elements().Reverse())
				{
					ExecutionStack.Push(Instruction);
				}
			}

			if (ExecutionStack.Count == 0)
			{
				return Input;
			}

			Stack<UPLContext> ContextStack = new Stack<UPLContext>();
			UPLContext CurrentContext = GlobalContext;

			// update Output in global context
			GlobalContext.StringVariables["Output"] = Input;

			Stack<XElement> ElementStack = new Stack<XElement>();
			XElement CurrentElement = XMLWork.Elements().First();

			// run the instructions
			while (ExecutionStack.Count > 0)
			{
				XElement Node = ExecutionStack.Pop();
				if (bGlobalTrace || CurrentContext.bTrace)
				{
					Logger.LogInformation("Execute: '{Node}'", TraceNodeString(CurrentContext, Node, bGlobalTraceFilename || CurrentContext.bTraceFilename));
				}
				switch (Node.Name.ToString())
				{
					case "trace":
						{
							string? TraceFilename = GetAttribute(CurrentContext, Node, "filename");
							if (TraceFilename != null)
							{
								CurrentContext.bTraceFilename = StringToBool(TraceFilename);
							}
							string? Enable = GetAttribute(CurrentContext, Node, "enable");
							if (Enable != null)
							{
								CurrentContext.bTrace = StringToBool(Enable);
								if (!bGlobalTrace && CurrentContext.bTrace)
								{
									Logger.LogInformation("Context: '{Context}' using Architecture='{Arch}', NodeName='{NodeName}', Input='{Input}'", CurrentContext.StringVariables["PluginDir"], Architecture, NodeName, Input);
								}
							}
						}
						break;

					case "dumpvars":
						{
							if (!bGlobalTrace && !CurrentContext.bTrace)
							{
								Logger.LogInformation("Context: '{Context}' using Architecture='{Arch}', NodeName='{NodeName}', Input='{Input}'", CurrentContext.StringVariables["PluginDir"], Architecture, NodeName, Input);
							}
							Logger.LogInformation("Variables:\n{Variables}", DumpContext(CurrentContext));
						}
						break;

					case "Context":
						{
							ContextStack.Push(CurrentContext);
							string? index = GetAttribute(CurrentContext, Node, "index");
							CurrentContext = Contexts[Architecture + "_" + index];
							ExecutionStack.Push(new XElement("PopContext"));
							foreach (XElement instruction in Node.Elements().Reverse())
							{
								ExecutionStack.Push(instruction);
							}

							if (bGlobalTrace || CurrentContext.bTrace)
							{
								Logger.LogInformation("Context: '{Context}' using Architecture='{Arch}', NodeName='{NodeName}', Input='{Input}'", CurrentContext.StringVariables["PluginDir"], Architecture, NodeName, Input);
							}
						}
						break;

					case "PopContext":
						{
							CurrentContext = ContextStack.Pop();
						}
						break;

					case "PopElement":
						{
							CurrentElement = ElementStack.Pop();
						}
						break;

					case "isArch":
						{
							string? arch = GetAttribute(CurrentContext, Node, "arch");
							if (arch != null && arch.Equals(Architecture))
							{
								foreach (XElement instruction in Node.Elements().Reverse())
								{
									ExecutionStack.Push(instruction);
								}
							}
						}
						break;

					case "isDistribution":
						{
							bool Result = false;
							if (GetCondition(CurrentContext, Node, "Distribution", out Result))
							{
								if (Result)
								{
									foreach (XElement Instruction in Node.Elements().Reverse())
									{
										ExecutionStack.Push(Instruction);
									}
								}
							}
						}
						break;

					case "if":
						{
							bool Result;
							if (GetCondition(CurrentContext, Node, GetAttribute(CurrentContext, Node, "condition"), out Result))
							{
								XElement? ResultNode = Node.Element(Result ? "true" : "false");
								if (ResultNode != null)
								{
									foreach (XElement Instruction in ResultNode.Elements().Reverse())
									{
										ExecutionStack.Push(Instruction);
									}
								}
							}
						}
						break;

					case "while":
						{
							bool Result;
							if (GetCondition(CurrentContext, Node, GetAttribute(CurrentContext, Node, "condition"), out Result))
							{
								if (Result)
								{
									IEnumerable<XElement> ResultNode = Node.Elements();
									if (ResultNode != null)
									{
										ExecutionStack.Push(Node);
										foreach (XElement Instruction in ResultNode.Reverse())
										{
											ExecutionStack.Push(Instruction);
										}
									}
								}
							}
						}
						break;

					case "return":
						{
							while (ExecutionStack.Count > 0)
							{
								ExecutionStack.Pop();
							}
						}
						break;

					case "break":
						{
							// remove up to while (or acts like a return if outside by removing everything)
							while (ExecutionStack.Count > 0)
							{
								Node = ExecutionStack.Pop();
								if (Node.Name.ToString().Equals("while"))
								{
									break;
								}
							}
						}
						break;

					case "continue":
						{
							// remove up to while (or acts like a return if outside by removing everything)
							while (ExecutionStack.Count > 0)
							{
								Node = ExecutionStack.Pop();
								if (Node.Name.ToString().Equals("while"))
								{
									ExecutionStack.Push(Node);
									break;
								}
							}
						}
						break;

					case "log":
						{
							string? Text = GetAttribute(CurrentContext, Node, "text");
							if (Text != null)
							{
								Logger.LogInformation("{Message}", Text);
							}
						}
						break;

					case "loopElements":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							ElementStack.Push(CurrentElement);
							ExecutionStack.Push(new XElement("PopElement"));
							IEnumerable<XElement> WorkList = (Tag == "$") ? CurrentElement.Elements().Reverse() : CurrentElement.Descendants(Tag).Reverse();
							foreach (XElement WorkNode in WorkList)
							{
								foreach (XElement Instruction in Node.Elements().Reverse())
								{
									ExecutionStack.Push(Instruction);
								}
								ElementStack.Push(WorkNode);
								ExecutionStack.Push(new XElement("PopElement"));
							}
						}
						break;

					case "addAttribute":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							string? Name = GetAttribute(CurrentContext, Node, "name");
							string? Value = GetAttribute(CurrentContext, Node, "value");
							if (Tag != null && Name != null && Value != null)
							{
								if (Tag.StartsWith("$"))
								{
									XElement? Target = CurrentElement;
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
											{
												Logger.LogWarning("Missing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
									AddAttribute(Target!, Name, Value);
								}
								else
								{
									if (CurrentElement.Name.ToString().Equals(Tag))
									{
										AddAttribute(CurrentElement, Name, Value);
									}
									foreach (XElement WorkNode in CurrentElement.Descendants(Tag))
									{
										AddAttribute(WorkNode, Name, Value);
									}
								}
							}
						}
						break;

					case "removeAttribute":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							string? Name = GetAttribute(CurrentContext, Node, "name");
							if (Tag != null && Name != null)
							{
								if (Tag.StartsWith("$"))
								{
									XElement? Target = CurrentElement;
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
											{
												Logger.LogInformation("\nMissing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
									RemoveAttribute(Target, Name);
								}
								else
								{
									if (CurrentElement.Name.ToString().Equals(Tag))
									{
										RemoveAttribute(CurrentElement, Name);
									}
									foreach (XElement WorkNode in CurrentElement.Descendants(Tag))
									{
										RemoveAttribute(WorkNode, Name);
									}
								}
							}
						}
						break;

					case "addPermission":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								// make sure it isn't already added
								bool bFound = false;
								foreach (XElement Element in XMLWork.Descendants("uses-permission"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											bFound = true;
											break;
										}
									}
								}

								// add it if not found
								if (!bFound)
								{
									// Get the attributes and apply any variable expansion needed
									List<XAttribute> AttributeList = Node.Attributes().ToList();
									foreach (XAttribute Attribute in AttributeList)
									{
										string NewValue = ExpandVariables(CurrentContext, Attribute.Value);
										Attribute.SetValue(NewValue);
									}

									XMLWork.Element("manifest")?.Add(new XElement("uses-permission", AttributeList));
								}
							}
						}
						break;

					case "removePermission":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								foreach (XElement Element in XMLWork.Descendants("uses-permission"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											Element.Remove();
											break;
										}
									}
								}
							}
						}
						break;

					case "addFeature":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								// make sure it isn't already added
								bool bFound = false;
								foreach (XElement Element in XMLWork.Descendants("uses-feature"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											bFound = true;
											break;
										}
									}
								}

								// add it if not found
								if (!bFound)
								{
									// Get the attributes and apply any variable expansion needed
									List<XAttribute> AttributeList = Node.Attributes().ToList();
									foreach (XAttribute Attribute in AttributeList)
									{
										string NewValue = ExpandVariables(CurrentContext, Attribute.Value);
										Attribute.SetValue(NewValue);
									}

									XMLWork.Element("manifest")?.Add(new XElement("uses-feature", AttributeList));
								}
							}
						}
						break;

					case "removeFeature":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								foreach (XElement Element in XMLWork.Descendants("uses-feature"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											Element.Remove();
											break;
										}
									}
								}
							}
						}
						break;

					case "addLibrary":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								// make sure it isn't already added
								bool bFound = false;
								foreach (XElement Element in XMLWork.Descendants("uses-library"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											bFound = true;
											break;
										}
									}
								}

								// add it if not found
								if (!bFound)
								{
									// Get the attributes and apply any variable expansion needed
									List<XAttribute> AttributeList = Node.Attributes().ToList();
									foreach (XAttribute Attribute in AttributeList)
									{
										string NewValue = ExpandVariables(CurrentContext, Attribute.Value);
										Attribute.SetValue(NewValue);
									}

									XMLWork.Element("manifest")?.Element("application")?.Add(new XElement("uses-library", AttributeList));
								}
							}
						}
						break;

					case "removeLibrary":
						{
							string? Name = GetAttributeWithNamespace(CurrentContext, Node, XMLDefaultNameSpace, "name");
							if (Name != null)
							{
								foreach (XElement Element in XMLWork.Descendants("uses-library"))
								{
									XAttribute? Attribute = Element.Attribute(XMLDefaultNameSpace + "name");
									if (Attribute != null)
									{
										if (Attribute.Value == Name)
										{
											Element.Remove();
											break;
										}
									}
								}
							}
						}
						break;

					case "removeElement":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							bool bOnce = StringToBool(GetAttribute(CurrentContext, Node, "once", true, false));
							if (Tag != null)
							{
								if (Tag == "$")
								{
									XElement Parent = CurrentElement.Parent!;
									CurrentElement.Remove();
									CurrentElement = Parent;
								}
								else
								{
									// use a list since Remove() may modify it
									foreach (XElement Element in XMLWork.Descendants(Tag).ToList())
									{
										Element.Remove();
										if (bOnce)
										{
											break;
										}
									}
								}
							}
						}
						break;

					case "addElement":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							string? Name = GetAttribute(CurrentContext, Node, "name");
							bool bOnce = StringToBool(GetAttribute(CurrentContext, Node, "once", true, false));
							if (Tag != null && Name != null)
							{
								XElement? Element;
								if (!CurrentContext.ElementVariables.TryGetValue(Name, out Element))
								{
									if (!GlobalContext.ElementVariables.TryGetValue(Name, out Element))
									{
										Logger.LogWarning("Missing element variable '{Name}' in '{Node}' (skipping instruction)", Name, TraceNodeString(CurrentContext, Node));
										continue;
									}
								}
								if (Tag.StartsWith("$"))
								{
									XElement? Target = CurrentElement;
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
											{
												Logger.LogWarning("Missing element variable '{Name}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
									Target.Add(new XElement(Element));
								}
								else
								{
									if (CurrentElement.Name.ToString().Equals(Tag))
									{
										CurrentElement.Add(new XElement(Element));
									}

									// make sure we don't recurse forever if Tag is in Element
									List<XElement> AddSet = new List<XElement>();
									foreach (XElement WorkNode in CurrentElement.Descendants(Tag))
									{
										AddSet.Add(WorkNode);
										if (bOnce)
										{
											break;
										}
									}
									foreach (XElement WorkNode in AddSet)
									{
										WorkNode.Add(new XElement(Element));
									}
								}
							}
						}
						break;

					case "addElements":
						{
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							bool bOnce = StringToBool(GetAttribute(CurrentContext, Node, "once", true, false));
							if (Tag != null)
							{
								if (Tag.StartsWith("$"))
								{
									XElement? Target = CurrentElement;
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Target))
											{
												Logger.LogWarning("Missing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
									AddElements(Target, Node);
								}
								else
								{
									if (CurrentElement.Name.ToString().Equals(Tag))
									{
										AddElements(CurrentElement, Node);
									}

									// make sure we don't recurse forever if Tag is in Node
									List<XElement> AddSet = new List<XElement>();
									foreach (XElement WorkNode in CurrentElement.Descendants(Tag))
									{
										AddSet.Add(WorkNode);
										if (bOnce)
										{
											break;
										}
									}
									foreach (XElement WorkNode in AddSet)
									{
										AddElements(WorkNode, Node);
									}
								}
							}
						}
						break;

					case "insert":
						{
							if (Node.HasElements)
							{
								foreach (XElement Element in Node.Elements())
								{
									string Value = Element.ToString().Replace(" " + XMLRootDefinition + " ", "");
									GlobalContext.StringVariables["Output"] += Value + "\n";
								}
							}
							else
							{
								string Value = Node.Value.ToString();

								// trim trailing tabs
								int Index = Value.Length;
								while (Index > 0 && Value[Index - 1] == '\t')
								{
									Index--;
								}
								if (Index < Value.Length)
								{
									Value = Value.Substring(0, Index);
								}

								// trim leading newlines
								Index = 0;
								while (Index < Value.Length && Value[Index] == '\n')
								{
									Index++;
								}

								if (Index < Value.Length)
								{
									GlobalContext.StringVariables["Output"] += Value.Substring(Index);
								}
							}
						}
						break;

					case "insertNewline":
						GlobalContext.StringVariables["Output"] += "\n";
						break;

					case "insertValue":
						{
							string? Value = GetAttribute(CurrentContext, Node, "value");
							if (Value != null)
							{
								GlobalContext.StringVariables["Output"] += Value;
							}
						}
						break;

					case "replace":
						{
							string? Find = GetAttribute(CurrentContext, Node, "find");
							string? With = GetAttribute(CurrentContext, Node, "with");
							if (Find != null && With != null)
							{
								GlobalContext.StringVariables["Output"] = GlobalContext.StringVariables["Output"].Replace(Find, With);
							}
						}
						break;

					case "copyFile":
						{
							string? Src = GetAttribute(CurrentContext, Node, "src");
							string? Dst = GetAttribute(CurrentContext, Node, "dst");
							bool bForce = StringToBool(GetAttribute(CurrentContext, Node, "force", true, false, "true"));
							if (Src != null && Dst != null)
							{
								if (File.Exists(Src))
								{
									// check to see if newer than last time we copied
									if (bForce || FilesAreDifferent(Src, Dst, Logger))
									{
										if (File.Exists(Dst))
										{
											File.SetAttributes(Dst, FileAttributes.Normal);
											File.Delete(Dst);
										}
										Directory.CreateDirectory(Path.GetDirectoryName(Dst)!);
										File.Copy(Src, Dst, true);
										Logger.LogInformation("File {Src} copied to {Dst}", Src, Dst);

										// remove any read only flags and keep timestamp
										FileInfo DestFileInfo = new FileInfo(Dst);
										DestFileInfo.Attributes &= ~FileAttributes.ReadOnly;
										File.SetLastWriteTimeUtc(Dst, File.GetLastWriteTimeUtc(Src));
									}
								}
								else
								{
									Logger.LogInformation("File {Src} does not exist, not copied!", Src);
								}
							}
						}
						break;

					case "copyDir":
						{
							string? Src = GetAttribute(CurrentContext, Node, "src");
							string? Dst = GetAttribute(CurrentContext, Node, "dst");
							bool bForce = StringToBool(GetAttribute(CurrentContext, Node, "force", true, false, "true"));
							if (Src != null && Dst != null)
							{
								CopyFileDirectory(Src, Dst, Logger, bForce);
								Logger.LogInformation("\nDirectory {Src} copied to {Dst} ({Force})", Src, Dst, bForce);
							}
						}
						break;

					case "deleteFiles":
						{
							string? Filespec = GetAttribute(CurrentContext, Node, "filespec");
							bool bRecursive = StringToBool(GetAttribute(CurrentContext, Node, "recursive", true, false, "false"));
							if (Filespec != null)
							{
								if (Filespec.Contains(':') || Filespec.Contains(".."))
								{
									Logger.LogInformation("\nFilespec {FileSpec} not allowed; ignored.", Filespec);
								}
								else
								{
									// force relative to BuildDir (and only from global context so someone doesn't try to be clever)
									DeleteFiles(Path.Combine(GlobalContext.StringVariables["BuildDir"], Filespec), bRecursive, Logger);
								}
							}
						}
						break;

					case "loadLibrary":
						{
							string? Name = GetAttribute(CurrentContext, Node, "name");
							string? FailMsg = GetAttribute(CurrentContext, Node, "failmsg", true, false);
							if (Name != null)
							{
								string Work = "\t\ttry\n" +
											"\t\t{\n" +
											"\t\t\tSystem.loadLibrary(\"" + Name + "\");\n" +
											"\t\t}\n" +
											"\t\tcatch (java.lang.UnsatisfiedLinkError e)\n" +
											"\t\t{\n";
								if (FailMsg != null)
								{
									Work += "\t\t\tLog.debug(e.toString());\n";
									Work += "\t\t\tLog.debug(\"" + FailMsg + "\");\n";
								}
								GlobalContext.StringVariables["Output"] += Work + "\t\t}\n";
							}
						}
						break;

					case "setBool":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "false");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = StringToBool(Value);
							}
						}
						break;

					case "setBoolEnvVarDefined":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Value = GetAttribute(CurrentContext, Node, "value");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = (Value != null && Environment.ExpandEnvironmentVariables(Value).Length > 0);
							}
						}
						break;

					case "setBoolFrom":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "false");
							if (Result != null)
							{
								Value = ExpandVariables(CurrentContext, "$B(" + Value + ")");
								CurrentContext.BoolVariables[Result] = StringToBool(Value);
							}
						}
						break;

					case "setBoolFromProperty":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "false");
							if (Result != null && Ini != null && Section != null && Property != null)
							{
								bool Value = StringToBool(DefaultVal);

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									if (!ConfigIni.GetBool(Section, Property, out Value))
									{
										Value = StringToBool(DefaultVal);
									}
								}
								CurrentContext.BoolVariables[Result] = Value;
							}
						}
						break;

					case "setBoolFromPropertyContains":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "false");
							string Contains = GetAttribute(CurrentContext, Node, "contains", true, true, "");
							if (Result != null && Ini != null && Section != null && Property != null)
							{
								bool Value = StringToBool(DefaultVal);

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									List<string>? StringList;
									if (ConfigIni.GetArray(Section, Property, out StringList))
									{
										Value = false;
										foreach (string Entry in StringList)
										{
											if (Entry.Equals(Contains))
											{
												Value = true;
												break;
											}
										}
									}
								}
								CurrentContext.BoolVariables[Result] = Value;
							}
						}
						break;

					case "setBoolContains":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Find = GetAttribute(CurrentContext, Node, "find", true, false, "");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = Source.Contains(Find);
							}
						}
						break;

					case "setBoolStartsWith":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Find = GetAttribute(CurrentContext, Node, "find", true, false, "");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = Source.StartsWith(Find);
							}
						}
						break;

					case "setBoolEndsWith":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Find = GetAttribute(CurrentContext, Node, "find", true, false, "");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = Source.EndsWith(Find);
							}
						}
						break;

					case "setBoolNot":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "false");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = !StringToBool(Source);
							}
						}
						break;

					case "setBoolAnd":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "false");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "false");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = StringToBool(Arg1) && StringToBool(Arg2);
							}
						}
						break;

					case "setBoolOr":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "false");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "false");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = StringToBool(Arg1) || StringToBool(Arg2);
							}
						}
						break;

					case "setBoolIsEqual":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "");
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = Arg1.Equals(Arg2);
							}
						}
						break;

					case "setBoolIsLess":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false);
							string? Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false);
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = (StringToInt(CurrentContext, Node, Arg1) < StringToInt(CurrentContext, Node, Arg2));
							}
						}
						break;

					case "setBoolIsLessEqual":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false);
							string? Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false);
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = (StringToInt(CurrentContext, Node, Arg1) <= StringToInt(CurrentContext, Node, Arg2));
							}
						}
						break;

					case "setBoolIsGreater":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false);
							string? Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false);
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = (StringToInt(CurrentContext, Node, Arg1) > StringToInt(CurrentContext, Node, Arg2));
							}
						}
						break;

					case "setBoolIsGreaterEqual":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false);
							string? Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false);
							if (Result != null)
							{
								CurrentContext.BoolVariables[Result] = (StringToInt(CurrentContext, Node, Arg1) >= StringToInt(CurrentContext, Node, Arg2));
							}
						}
						break;

					case "setBoolFileExists":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? FilePath = GetAttribute(CurrentContext, Node, "file");
							if (Result != null && FilePath != null)
							{
								CurrentContext.BoolVariables[Result] = File.Exists(FilePath);
							}
						}
						break;

					case "setInt":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "0");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Value);
							}
						}
						break;

					case "setIntFrom":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "0");
							if (Result != null)
							{
								Value = ExpandVariables(CurrentContext, "$I(" + Value + ")");
								CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Value);
							}
						}
						break;

					case "setIntFromProperty":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "0");
							if (Result != null && Ini != null && Section != null && Property != null)
							{
								int DefaultInt = StringToInt(CurrentContext, Node, DefaultVal);
								int Value = DefaultInt;

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									if (!ConfigIni.GetInt32(Section, Property, out Value))
									{
										Value = DefaultInt;
									}
								}
								CurrentContext.IntVariables[Result] = Value;
							}
						}
						break;

					case "setIntFromPropertyArrayNum":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							if (Result != null && Ini != null && Section != null && Property != null)
							{
								int Value = 0;

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									List<string>? StringList;
									if (ConfigIni.GetArray(Section, Property, out StringList))
									{
										Value = StringList.Count;
									}
								}
								CurrentContext.IntVariables[Result] = Value;
							}
						}
						break;

					case "setIntAdd":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "0");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "0");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Arg1) + StringToInt(CurrentContext, Node, Arg2);
							}
						}
						break;

					case "setIntSubtract":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "0");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "0");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Arg1) - StringToInt(CurrentContext, Node, Arg2);
							}
						}
						break;

					case "setIntMultiply":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "1");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "1");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Arg1) * StringToInt(CurrentContext, Node, Arg2);
							}
						}
						break;

					case "setIntDivide":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "1");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "1");
							if (Result != null)
							{
								int Denominator = StringToInt(CurrentContext, Node, Arg2);
								if (Denominator == 0)
								{
									CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Arg1);
								}
								else
								{
									CurrentContext.IntVariables[Result] = StringToInt(CurrentContext, Node, Arg1) / Denominator;
								}
							}
						}
						break;

					case "setIntLength":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = Source.Length;
							}
						}
						break;

					case "setIntFindString":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Find = GetAttribute(CurrentContext, Node, "find", true, false, "");
							if (Result != null)
							{
								CurrentContext.IntVariables[Result] = Source.IndexOf(Find);
							}
						}
						break;

					case "setString":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "");
							if (Result != null)
							{
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringFrom":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Value = GetAttribute(CurrentContext, Node, "value", true, false, "0");
							if (Result != null)
							{
								Value = ExpandVariables(CurrentContext, "$S(" + Value + ")");
								CurrentContext.StringVariables[Result] = Value;
							}
						}
						break;

					case "setStringFromEnvVar":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Value = GetAttribute(CurrentContext, Node, "value");
							if (Result != null && Value != null)
							{
								CurrentContext.StringVariables[Result] = Environment.ExpandEnvironmentVariables(Value);
							}
						}
						break;

					case "setStringFromTag":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Tag = GetAttribute(CurrentContext, Node, "tag", true, false, "$");
							if (Result != null)
							{
								XElement? Element = CurrentElement;
								if (Tag.StartsWith("$"))
								{
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
											{
												Logger.LogWarning("Missing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
								}
								CurrentContext.StringVariables[Result] = Element.Name.ToString();
							}
						}
						break;

					case "setStringFromAttribute":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Tag = GetAttribute(CurrentContext, Node, "tag");
							string? Name = GetAttribute(CurrentContext, Node, "name");
							if (Result != null && Tag != null && Name != null)
							{
								XElement? Element = CurrentElement;
								if (Tag.StartsWith("$"))
								{
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
											{
												Logger.LogWarning("Missing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
								}

								XNamespace? XMLNameSpace = TrimNamespaceAliasFromName(ref Name);
								XAttribute? Attribute = Element.Attribute(XMLNameSpace != null ? XMLNameSpace + Name : Name);

								CurrentContext.StringVariables[Result] = (Attribute != null) ? Attribute.Value : "";
							}
						}
						break;

					case "setStringFromTagText":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Tag = GetAttribute(CurrentContext, Node, "tag", true, false, "$");
							if (Result != null)
							{
								XElement? Element = CurrentElement;
								if (Tag.StartsWith("$"))
								{
									if (Tag.Length > 1)
									{
										if (!CurrentContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
										{
											if (!GlobalContext.ElementVariables.TryGetValue(Tag.Substring(1), out Element))
											{
												Logger.LogWarning("Missing element variable '{Tag}' in '{Node}' (skipping instruction)", Tag, TraceNodeString(CurrentContext, Node));
												continue;
											}
										}
									}
								}

								if (Element.Value == null)
								{
									Logger.LogWarning("Expected text in element '{Element}' in '{Node}' but found none (skipping instruction)", Element.Name.ToString(), TraceNodeString(CurrentContext, Node));
									continue;
								}

								CurrentContext.StringVariables[Result] = Element.Value;
							}
						}
						break;

					case "setStringFromProperty":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "");
							if (Result != null && Ini != null && Section != null && Property != null)
							{
								string Value = DefaultVal;

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									if (!ConfigIni.GetString(Section, Property, out Value))
									{
										// If the string was not found in the config, Value will have been set to an empty string
										// Set it back to the DefaultVal
										Value = DefaultVal;
									}
								}
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringFromPropertyArray":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Ini = GetAttribute(CurrentContext, Node, "ini");
							string? Section = GetAttribute(CurrentContext, Node, "section");
							string? Property = GetAttribute(CurrentContext, Node, "property");
							string? IndexStr = GetAttribute(CurrentContext, Node, "index", true);
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "");
							if (Result != null && Ini != null && Section != null && Property != null && IndexStr != null)
							{
								string Value = DefaultVal;

								ConfigCacheIni_UPL ConfigIni = GetConfigCacheIni_UPL(Ini);
								if (ConfigIni != null)
								{
									List<string>? StringList;
									if (ConfigIni.GetArray(Section, Property, out StringList))
									{
										int Index = StringToInt(CurrentContext, Node, IndexStr);
										if (Index >= 0 && Index < StringList.Count)
										{
											Value = StringList.ElementAt(Index);
										}
									}
								}
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringAdd":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Arg1 = GetAttribute(CurrentContext, Node, "arg1", true, false, "");
							string Arg2 = GetAttribute(CurrentContext, Node, "arg2", true, false, "");
							if (Result != null)
							{
								string Value = Arg1 + Arg2;
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringSubstring":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Start = GetAttribute(CurrentContext, Node, "start", true, false, "0");
							string Length = GetAttribute(CurrentContext, Node, "length", true, false, "0");
							if (Result != null && Source != null)
							{
								int Index = StringToInt(CurrentContext, Node, Start);
								int Count = StringToInt(CurrentContext, Node, Length);
								Index = (Index < 0) ? 0 : (Index > Source.Length) ? Source.Length : Index;
								Count = (Index + Count > Source.Length) ? Source.Length - Index : Count;
								string Value = Source.Substring(Index, Count);
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringSubstringAfterFind":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string Find = GetAttribute(CurrentContext, Node, "find", true, false, "");
							string Length = GetAttribute(CurrentContext, Node, "length", true, false, "-1");
							string DefaultVal = GetAttribute(CurrentContext, Node, "default", true, false, "");
							if (Result != null)
							{
								string Value = DefaultVal;
								int Index = Source.IndexOf(Find);
								if (Index != -1)
								{
									Index += Find.Length;
									int Count = StringToInt(CurrentContext, Node, Length);
									Count = Count == -1 || (Index + Count > Source.Length) ? Source.Length - Index : Count;
									Value = Source.Substring(Index, Count);
								}
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringReplace":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							string? Find = GetAttribute(CurrentContext, Node, "find");
							string With = GetAttribute(CurrentContext, Node, "with", true, false, "");
							if (Result != null && Find != null)
							{
								string Value = Source.Replace(Find, With);
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringToLower":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							if (Result != null)
							{
								string Value = Source.ToLower();
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setStringToUpper":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string Source = GetAttribute(CurrentContext, Node, "source", true, false, "");
							if (Result != null)
							{
								string Value = Source.ToUpper();
								if (Result == "Output")
								{
									GlobalContext.StringVariables["Output"] = Value;
								}
								else
								{
									CurrentContext.StringVariables[Result] = Value;
								}
							}
						}
						break;

					case "setElement":
						{
							string? Result = GetAttribute(CurrentContext, Node, "result");
							string? Value = GetAttribute(CurrentContext, Node, "value", true, false);
							string? Text = GetAttribute(CurrentContext, Node, "text", true, false);
							string? Parse = GetAttribute(CurrentContext, Node, "xml", true, false);
							if (Result != null)
							{
								if (Value != null)
								{
									XNamespace? XMLNameSpace = TrimNamespaceAliasFromName(ref Value);
									XElement NewElement = new XElement(XMLNameSpace != null ? XMLNameSpace + Value : Value);
									if (Text != null)
									{
										NewElement.Value = Text;
									}
									CurrentContext.ElementVariables[Result] = NewElement;
								}
								else if (Parse != null)
								{
									try
									{
										CurrentContext.ElementVariables[Result] = XElement.Parse(Parse);
									}
									catch (Exception e)
									{
										Logger.LogError(e, "XML parsing {Parse} failed! {Ex} (skipping instruction)", Parse, e);
									}
								}
							}
						}
						break;

					// HPL字段获取
					case "addBackgroundModes":
						{
							Logger.LogInformation("\nLog UPL: Node.Value \"{0}\"", Node.Value);
							string[] Values = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							if (!GlobalContext.StringValueUpdates.ContainsKey("BackgroundModes")) 
							{
								GlobalContext.StringValueUpdates.Add("BackgroundModes", new HashSet<string>());
							}
							foreach (var value in Values) 
							{
								GlobalContext.StringValueUpdates["BackgroundModes"].Add(value.Trim());
							}
						}
						break;
					
					case "addRequestPermissions":
						{
							string[] Values = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							foreach (var permission in Values)
							{
								RequestPermission permissionTemp = new RequestPermission();
								if (permissionTemp.name == "") continue;
								if (!GlobalContext.AddPermissions.Contains(permissionTemp))
								{
									GlobalContext.AddPermissions.Add(permissionTemp);
								}
							}
						} 
						break;
					
					case "addCustomRequestPermission":
						{
							string? Name = GetAttribute(CurrentContext, Node, "name");
							string? Reason = GetAttribute(CurrentContext, Node, "reason");
							string? UsedSceneWhen = GetAttribute(CurrentContext, Node, "usedScene_when");
							
							FirstLevelSubNodeProcessor processor = new FirstLevelSubNodeProcessor(this);
							processor.ProcessNode(Node);
							
							// 如果使用$string:前缀，则直接写入，否则使用一个替代值进行映射
							// 若没有$string:前缀且不使用替代值编译时会报错
							if (Reason != null && !Reason.StartsWith("$string:") && !GlobalContext.ReasonMap.ReasonDict.ContainsKey(Reason))
							{
								string tempReason = "CustomReason_" + (GlobalContext.ReasonMap.Index++).ToString().PadLeft(3, '0');
								GlobalContext.ReasonMap.ReasonDict.Add(Reason, tempReason);
								Reason = "$string:" + tempReason;
							}
							
							RequestPermission permissionTemp = new RequestPermission();
							if(Name != null) permissionTemp.name = Name;
							if(Reason != null) permissionTemp.reason = Reason;
							if(UsedSceneWhen != null) permissionTemp.usedScene_when = UsedSceneWhen;
							permissionTemp.usedScene_abilities =
								GlobalContext.StringValueUpdates.TryGetValue("UsedSceneAbilities", out var usedSceneAbilities)
									? usedSceneAbilities.ToArray()
									: new String[] { "EntryAbility" };

							if (GlobalContext.AddPermissions.Contains(permissionTemp))
							{
								GlobalContext.AddPermissions.Remove(permissionTemp);
							}
							GlobalContext.AddPermissions.Add(permissionTemp);
						}
						break;

					case "addMetaData":
						{
							string? Name = GetAttribute(CurrentContext, Node, "name") ?? String.Empty;
							string? Value = GetAttribute(CurrentContext, Node, "value") ?? String.Empty;
							string? Resource = GetAttribute(CurrentContext, Node, "resource") ?? String.Empty;

							MetaData metaDataTemp = new MetaData();
							metaDataTemp.name = Name;
							metaDataTemp.value = Value;
							metaDataTemp.resource = Resource;
							
							GlobalContext.MetaDataSet.Add(metaDataTemp);
						}
						break;

					case "addPackageOverrides":
						{
							string? Name = GetAttribute(CurrentContext, Node, "name") ?? String.Empty;
							string? Override = GetAttribute(CurrentContext, Node, "override") ?? String.Empty;

							string HarDirectory = Path.Combine(".", "entry", "hars");
							string HarPackageName = Name + ".har";
							
							if (!GlobalContext.StringPairUpdates.ContainsKey("PackageOverrides"))
							{
								GlobalContext.StringPairUpdates.Add("PackageOverrides", new Dictionary<string, string>());
							}

							if (Override == String.Empty)
							{
								//当前har包路径写死的: ./entry/hars
								GlobalContext.StringPairUpdates["PackageOverrides"]
									.Add(HarPackageName.ToLower(), Path.Combine(HarDirectory, HarPackageName));
							}
							else
							{
								GlobalContext.StringPairUpdates["PackageOverrides"].Add(Name, Override);
							}
						}
						break;

					case "addPackageDependencies":
						{
							string? Plugin = GetAttribute(CurrentContext, Node, "plugin") ?? String.Empty;
							string? Dependency = GetAttribute(CurrentContext, Node, "dependency") ?? String.Empty;
							
							if (!GlobalContext.StringPairUpdates.ContainsKey("PackageDependencies"))
							{
								GlobalContext.StringPairUpdates.Add("PackageDependencies", new Dictionary<string, string>());
							}
							GlobalContext.StringPairUpdates["PackageDependencies"].Add(Plugin, Dependency);
						}
						break;

					case "addHarPackageNames":
						{
							string[] Values = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							if (!GlobalContext.StringValueUpdates.ContainsKey("HarPackageNames")) 
							{
								GlobalContext.StringValueUpdates.Add("HarPackageNames", new HashSet<string>());
							}
							foreach (var value in Values) 
							{
								GlobalContext.StringValueUpdates["HarPackageNames"].Add(value.Trim());
							}
						}
						break;
					case "addSkill":
						{
							string? DomainVerify = GetAttribute(CurrentContext, Node, "domainVerify") ?? "false";
							FirstLevelSubNodeProcessor processor = new FirstLevelSubNodeProcessor(this);
							processor.ProcessNode(Node);

							var actionSet = GlobalContext.StringValueUpdates["SkillActions"];
							var entitiesSet = GlobalContext.StringValueUpdates["SkillEntities"];
							var uriList = new List<UriInSkill>(GlobalContext.UriList.ToArray());
							var permissionSet = GlobalContext.StringValueUpdates["SkillPermissions"];
							
							AbilitySkill abilitySkill = new AbilitySkill(actionSet, entitiesSet, uriList, permissionSet, StringToBool(DomainVerify));
							GlobalContext.SkillList.Add(abilitySkill);
						}
						break;

					case "addQuerySchemes":
						{
							string[] Values = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							if (!GlobalContext.StringValueUpdates.ContainsKey("QuerySchemes")) 
							{
								GlobalContext.StringValueUpdates.Add("QuerySchemes", new HashSet<string>());
							}
							foreach (var value in Values) 
							{
								GlobalContext.StringValueUpdates["QuerySchemes"].Add(value.Trim());
							}
						}
						break;

					case "addRegistry":
						{
							string[] Values = Node.Value.Trim().Split(new[] { ",", "\r\n", "\r", "\n" }, StringSplitOptions.RemoveEmptyEntries);
							if (!GlobalContext.StringValueUpdates.ContainsKey("OhpmrcRegistry")) 
							{
								Logger.LogError("'OhpmrcRegistry' is not exist in StringValueUpdates");
							}
							foreach (var value in Values) 
							{
								GlobalContext.StringValueUpdates["OhpmrcRegistry"].Add(value.Trim());
							}
						}
						break;
					default:
						Logger.LogWarning("Unknown command: {Name}", Node.Name);
						break;
				}
			}

			return GlobalContext.StringVariables["Output"];
		}

		public void Init(List<string> Architectures, bool bDistribution, string EngineDirectory, string BuildDirectory, string ProjectDirectory, string Configuration, bool bIsEmbedded,
			bool bPerArchBuildDir = false, Dictionary<string, string>? ArchRemapping = null)
		{
			GlobalContext.BoolVariables["Distribution"] = bDistribution;
			GlobalContext.BoolVariables["IsEmbedded"] = bIsEmbedded;
			GlobalContext.StringVariables["Configuration"] = Configuration;

			GlobalContext.StringVariables["EngineDir"] = EngineDirectory.Replace("\\", "/");
			GlobalContext.StringVariables["BuildDir"] = BuildDirectory.Replace("\\", "/");
			GlobalContext.StringVariables["ProjectDir"] = ProjectDirectory.Replace("\\", "/");

			ReadOnlyBuildVersion Version = ReadOnlyBuildVersion.Current;
			GlobalContext.IntVariables["EngineMajorVersion"] = Version.MajorVersion;
			GlobalContext.IntVariables["EngineMinorVersion"] = Version.MinorVersion;
			GlobalContext.IntVariables["EnginePatchVersion"] = Version.PatchVersion;
			GlobalContext.IntVariables["EngineChangelist"] = Version.Changelist;
			GlobalContext.StringVariables["EngineVersion"] = Version.MajorVersion.ToString() + "." + Version.MinorVersion.ToString() + "." + Version.PatchVersion.ToString();
			GlobalContext.StringVariables["EngineBranchName"] = Version.BranchName ?? "";

			if (GlobalContext.StringVariables["EngineDir"].Length < 1)
			{
				GlobalContext.StringVariables["EngineDir"] = "./";
			}
			if (GlobalContext.StringVariables["BuildDir"].Length < 1)
			{
				GlobalContext.StringVariables["BuildDir"] = "./";
			}
			if (GlobalContext.StringVariables["ProjectDir"].Length < 1)
			{
				GlobalContext.StringVariables["ProjectDir"] = "./";
			}

			GlobalContext.StringVariables["AbsEngineDir"] = Path.GetFullPath(GlobalContext.StringVariables["EngineDir"]).Replace("\\", "/");
			GlobalContext.StringVariables["AbsBuildDir"] = Path.GetFullPath(GlobalContext.StringVariables["BuildDir"]).Replace("\\", "/");
			GlobalContext.StringVariables["AbsProjectDir"] = Path.GetFullPath(GlobalContext.StringVariables["ProjectDir"]).Replace("\\", "/");

			foreach (string Arch in Architectures)
			{
				if (bPerArchBuildDir)
				{
					string ActiveArch = Arch;
					if (ArchRemapping != null && ArchRemapping.ContainsKey(ActiveArch))
					{
						ActiveArch = ArchRemapping[ActiveArch];
					}
					string ArchBuildDirectory = Path.Combine(GlobalContext.StringVariables["BuildDir"], ActiveArch.Replace("-", "_"));
					string ArchBuildDir = ArchBuildDirectory.Replace("\\", "/");
					string ArchAbsBuildDir = Path.GetFullPath(ArchBuildDir).Replace("\\", "/");

					// add it to all the architecture contexts (overrides global context)
					for (int Index = 1; Index <= ContextIndex; Index++)
					{
						UPLContext ArchContext = Contexts[Arch + "_" + Index];
						ArchContext.StringVariables["BuildDir"] = ArchBuildDir;
						ArchContext.StringVariables["AbsBuildDir"] = ArchAbsBuildDir;
					}
				}

				Logger.LogInformation("UPL Init: {Arch}", Arch);
				ProcessPluginNode(Arch, "init", "");
			}

			if (bGlobalTrace)
			{
				Logger.LogInformation("\nVariables:\n{Variables}", DumpVariables());
			}
		}

		public void SetGlobalContextVariable(string VariableName, bool Value)
		{
			if (VariableName != null)
			{
				GlobalContext.BoolVariables[VariableName] = Value;
			}
		}

		public void SetGlobalContextVariable(string VariableName, int Value)
		{
			if (VariableName != null)
			{
				GlobalContext.IntVariables[VariableName] = Value;
			}
		}

		public void SetGlobalContextVariable(string VariableName, string Value)
		{
			if (VariableName != null && Value != null)
			{
				GlobalContext.StringVariables[VariableName] = Value;
			}
		}
		
	    private bool SafeDeleteFile(string Filename, bool bCheckExists = true)
	    {
		    if (!bCheckExists || File.Exists(Filename))
		    {
			    try
			    {
				    File.SetAttributes(Filename, FileAttributes.Normal);
				    File.Delete(Filename);
				    return true;
			    }
			    catch (System.UnauthorizedAccessException)
			    {
				    throw new BuildException("File '{0}' is in use; unable to modify it.", Filename);
			    }
			    catch (System.Exception)
			    {
				    return false;
			    }
		    }
		    return true;
	    }
	    
	    private void CleanCopyFile(string SourceFileName, string FinalFileName)
	    {
		    if (!File.Exists(SourceFileName))
		    {
			    return;
		    }
		    bool bFileExists = File.Exists(FinalFileName);
		    TimeSpan Diff = File.GetLastWriteTimeUtc(FinalFileName) - File.GetLastWriteTimeUtc(SourceFileName);
		    if (!bFileExists || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
		    {
			    SafeDeleteFile(FinalFileName);
			    Directory.CreateDirectory(Path.GetDirectoryName(FinalFileName)!);
			    File.Copy(SourceFileName, FinalFileName, true);

			    // make sure it's writable if the source was readonly (e.g. autosdks)
			    new FileInfo(FinalFileName).IsReadOnly = false;
			    File.SetLastWriteTimeUtc(FinalFileName, File.GetLastWriteTimeUtc(SourceFileName));
		    }
	    }
	    
	    private string GetOrientation()
	    {
		    string Orientation;
		    EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "Orientation",
			    out Orientation);
		    // check for UPL override
		    string OrientationOverride = ProcessPluginNode("arm64-v8a", "orientationOverride", "");
			if (!String.IsNullOrEmpty(OrientationOverride))
		    {
			    Orientation = OrientationOverride;
		    }

			return Orientation.ToLower() switch
			{
				"unspecified" => "unspecified",
				"portrait" => "portrait",
				"portrait_inverted" => "portrait_inverted",
				"landscape_inverted" => "landscape_inverted",
				"auto_rotation" => "auto_rotation",
				"auto_rotation_portrait" => "auto_rotation_portrait",
				"auto_rotation_landscape" => "auto_rotation_landscape",
				"auto_rotation_restricted" => "auto_rotation_restricted",
				"auto_rotation_portrait_restricted" => "auto_rotation_portrait_restricted",
				"auto_rotation_landscape_restricted" => "auto_rotation_landscape_restricted",
				"locked" => "locked",
				_ => "landscape",
			};
		}
	    
	    private void MakeDirectoryIfRequired(string DestFilename)
	    {
		    string DestSubdir = Path.GetDirectoryName(DestFilename)!;
		    if (!Directory.Exists(DestSubdir))
		    {
			    Directory.CreateDirectory(DestSubdir);
		    }
	    }

	    #region HPL配置文件写入相关函数
	    
	    public void ReplaceAppJson(string PackageName, string IntermediateOpenHarmonyPath)
		{
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			ConfigHierarchy GameIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
			string Vendor = "";
			int VersionCode = 1;
			string VersionName = "1.0.0.0";
			GameIni.GetString("/Script/EngineSettings.GeneralProjectSettings", "CompanyName", out Vendor);
			EngineIni.GetInt32("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "VersionCode", out VersionCode);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "VersionDisplayName", out VersionName);

			string appJsonFile = Path.Combine(IntermediateOpenHarmonyPath, "AppScope", "app.json5");
			using (JsonWriter JsonWriter = new JsonWriter(appJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteObjectStart("app");
				JsonWriter.WriteValue("bundleName", PackageName);
				JsonWriter.WriteValue("vendor", Vendor);
				JsonWriter.WriteValue("versionCode", VersionCode);
				JsonWriter.WriteValue("versionName", VersionName);
				JsonWriter.WriteValue("icon", "$media:app_icon");
				JsonWriter.WriteValue("label", "$string:app_name");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}

			string appElementFile = Path.Combine(IntermediateOpenHarmonyPath, "AppScope", "resources", "base", "element", "string.json");
			string AppName;
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AppName);
			if (string.IsNullOrEmpty(AppName))
			{
				AppName = "ueGame";
			}
			
			using (JsonWriter JsonWriter = new JsonWriter(appElementFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("string");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "app_name");
				JsonWriter.WriteValue("value", AppName);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}

			string DesIconFile = Path.Combine(IntermediateOpenHarmonyPath, "AppScope", "resources", "base", "media", "app_icon.png");
			string SourceIconFile;
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppIcon", out SourceIconFile);
			CleanCopyFile(SourceIconFile, DesIconFile);	
		}
    
	    private void WriteRequestPermissions(JsonWriter JsonWriter)
	    {
			JsonWriter.WriteArrayStart("requestPermissions");
			foreach (var permission in GlobalContext.AddPermissions)
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", permission.name);
				JsonWriter.WriteValue("reason", permission.reason);
				JsonWriter.WriteObjectStart("usedScene");
				JsonWriter.WriteArrayStart("abilities");
				foreach (var ability in permission.usedScene_abilities)
				{
					JsonWriter.WriteValue(ability);
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteValue("when", permission.usedScene_when);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}
			JsonWriter.WriteArrayEnd();
		}
	    
		private void WriteDefaultSkill(JsonWriter JsonWriter) 
		{
			JsonWriter.WriteObjectStart();
			JsonWriter.WriteArrayStart("entities");
			JsonWriter.WriteValue("entity.system.home");
			JsonWriter.WriteArrayEnd();
			JsonWriter.WriteArrayStart("actions");
			JsonWriter.WriteValue("action.system.home");
			JsonWriter.WriteArrayEnd();
			JsonWriter.WriteObjectEnd();
		}
	    
	    public void ReplaceModuleJson(string IntermediateOpenHarmonyPath)
		{
			string ModuleJsonPath = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "module.json5");
			ProcessPluginNode("arm64-v8a", "OHModuleJsonUpdate", "");
			using (JsonWriter JsonWriter = new JsonWriter(ModuleJsonPath))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteObjectStart("module");
				JsonWriter.WriteValue("name", "entry");
				JsonWriter.WriteValue("type", "entry");
				JsonWriter.WriteValue("description", "$string:module_desc");
				JsonWriter.WriteValue("mainElement", "EntryAbility");
				JsonWriter.WriteArrayStart("deviceTypes");
				JsonWriter.WriteValue("phone");
				JsonWriter.WriteValue("tablet");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteValue("deliveryWithInstall", true);
				JsonWriter.WriteValue("installationFree", false);
				JsonWriter.WriteValue("pages", "$profile:main_pages");
				JsonWriter.WriteArrayStart("abilities");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "EntryAbility");
				JsonWriter.WriteValue("srcEntry", "./ets/entryability/EntryAbility.ets");
				JsonWriter.WriteValue("orientation", GetOrientation());
				JsonWriter.WriteValue("description", "$string:EntryAbility_desc");
				JsonWriter.WriteValue("icon", "$media:ability_icon");
				JsonWriter.WriteValue("label", "$string:EntryAbility_label");
				JsonWriter.WriteValue("startWindowIcon", "$media:Start_image");
				JsonWriter.WriteValue("startWindowBackground", "$color:black");
				JsonWriter.WriteValue("exported", true);
				JsonWriter.WriteArrayStart("backgroundModes");
				if (GlobalContext.StringValueUpdates.ContainsKey("BackgroundModes"))
				{
					foreach (var mode in GlobalContext.StringValueUpdates["BackgroundModes"])
					{
						if(mode.Length > 0)
							JsonWriter.WriteValue(mode);
					}
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteArrayStart("skills");
				if (GlobalContext.SkillList.Count <= 0)
				{
					WriteDefaultSkill(JsonWriter);
				}
				else
				{
					foreach (var skill in GlobalContext.SkillList)
					{
						JsonWriter.WriteObjectStart();
						JsonWriter.WriteArrayStart("actions");
						foreach (var action in skill.Actions)
						{
							JsonWriter.WriteValue(action);
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteArrayStart("entities");
						foreach (var entity in skill.Entities)
						{
							JsonWriter.WriteValue(entity);
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteArrayStart("uris");
						foreach (var uri in skill.Uris)
						{
							JsonWriter.WriteObjectStart();
							if(uri.scheme != String.Empty) JsonWriter.WriteValue("scheme", uri.scheme);
							if(uri.host != String.Empty) JsonWriter.WriteValue("host", uri.host);
							if(uri.port != String.Empty) JsonWriter.WriteValue("port", uri.port);
							if (uri.path != String.Empty)
							{
								JsonWriter.WriteValue("path", uri.path);
							}
							else if (uri.pathStartWith != String.Empty)
							{
								JsonWriter.WriteValue("pathStartWith", uri.pathStartWith);
							}
							else if (uri.pathRegex != String.Empty)
							{
								JsonWriter.WriteValue("pathRegex", uri.pathRegex);
							}
							if(uri.type != String.Empty) JsonWriter.WriteValue("type", uri.type);
							if(uri.utd != String.Empty) JsonWriter.WriteValue("utd", uri.utd);
							if(uri.maxFileSupported != 0) JsonWriter.WriteValue("maxFileSupported", uri.maxFileSupported);
							if(uri.linkFeature != String.Empty) JsonWriter.WriteValue("linkFeature", uri.linkFeature);
							JsonWriter.WriteObjectEnd();
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteArrayStart("permissions");
						foreach (var permission in skill.Permissions)
						{
							JsonWriter.WriteValue(permission);
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteValue("domainVerify", skill.DomainVerify);
						JsonWriter.WriteObjectEnd();
					}
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				
				// allow plugins to modify final modulejson HERE
				WriteRequestPermissions(JsonWriter);
				
				//ClientID和APPID
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.OpenHarmony);
				string ClientID, APPID;
				Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "ClientID", out ClientID);
				Ini.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "APPID", out APPID);
				
				JsonWriter.WriteArrayStart("metadata");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "client_id");
				JsonWriter.WriteValue("value", ClientID);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "app_id");
				JsonWriter.WriteValue("value", APPID);
				JsonWriter.WriteObjectEnd();
				foreach (var metadata in GlobalContext.MetaDataSet)
				{
					JsonWriter.WriteObjectStart();
					JsonWriter.WriteValue("name", metadata.name);
					JsonWriter.WriteValue("value", metadata.value);
					if(metadata.resource != String.Empty)
					{
						JsonWriter.WriteValue("resource", metadata.resource);
					}
					JsonWriter.WriteObjectEnd();
				}
				JsonWriter.WriteArrayEnd();
				if (GlobalContext.StringValueUpdates.TryGetValue("QuerySchemes", out var QuerySchemeSet))
				{
					JsonWriter.WriteArrayStart("querySchemes");
					foreach (var queryScheme in QuerySchemeSet)
					{
						JsonWriter.WriteValue(queryScheme.Trim());
					}
					JsonWriter.WriteArrayEnd();
				}
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
			}
		}
	    
		private void JsonWriteString(JsonWriter JsonWriter, string name, string value, string defaultValue)
		{
			JsonWriter.WriteObjectStart();
			JsonWriter.WriteValue("name", name);
			JsonWriter.WriteValue("value", string.IsNullOrEmpty(value) ? defaultValue : value);
			JsonWriter.WriteObjectEnd();
		}
	    
	    public void ReplaceElementJson(string ProjectName, string IntermediateOpenHarmonyPath)
		{ 
			string ModuleDesc, AbilityDesc, AbilityLabel, AbilityReason;
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "ModuleDesc", out ModuleDesc);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AbilityDesc", out AbilityDesc);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AbilityLabel);
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AbilityReason", out AbilityReason);
			if (string.IsNullOrEmpty(AbilityLabel))
			{
				AbilityLabel = "ueGame";
			}
			bool bShowLaunchImage = false;
			EngineIni.GetBool("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);

			string ModuleStringJsonFile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "base", "element", "string.json");
			using (JsonWriter JsonWriter = new JsonWriter(ModuleStringJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("string");
				JsonWriteString(JsonWriter, "module_desc", ModuleDesc, "description");
				JsonWriteString(JsonWriter, "EntryAbility_desc", AbilityDesc, "description");
				JsonWriteString(JsonWriter, "EntryAbility_label", AbilityLabel, "label");
				JsonWriteString(JsonWriter, "reason", AbilityReason, "reason");
				foreach (var kvp in GlobalContext.ReasonMap.ReasonDict)
				{
					string reasonName = kvp.Value, reasonValue = kvp.Key;
					JsonWriteString(JsonWriter, reasonName, reasonValue, "reason");
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}
			
			string ModuleBooleanJsonFile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "base", "element", "boolean.json");
			using (JsonWriter JsonWriter = new JsonWriter(ModuleBooleanJsonFile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteArrayStart("boolean");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "bUseSplash");
				JsonWriter.WriteValue("value", bShowLaunchImage);
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}

			string DesIconFile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "src", "main", "resources", "base", "media", "ability_icon.png");
			EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppIcon", out var SourceIconFile);
			CleanCopyFile(SourceIconFile, DesIconFile);
		}
	    
	    public void ReplaceBuildProfile(string IntermediateOpenHarmonyPath)
	    {
		    string BuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "build-profile.json5");
		    using (JsonWriter JsonWriter = new JsonWriter(BuildProfile))
		    {
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteObjectStart("app");
			    JsonWriter.WriteArrayStart("signingConfigs");
				
			    JsonWriter.WriteArrayEnd();

			    JsonWriter.WriteArrayStart("products");
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteValue("name", "default");
			    JsonWriter.WriteValue("signingConfig", "default");
			    JsonWriter.WriteValue("compatibleSdkVersion", "5.0.0(12)");
			    JsonWriter.WriteValue("runtimeOS", "HarmonyOS");
			    JsonWriter.WriteObjectStart("buildOption");
			    JsonWriter.WriteObjectStart("strictMode");
			    JsonWriter.WriteValue("useNormalizedOHMUrl", true);
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteArrayEnd();
			    JsonWriter.WriteObjectEnd();

			    JsonWriter.WriteArrayStart("modules");
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteValue("name", "entry");
			    JsonWriter.WriteValue("srcPath", "./entry");
			    JsonWriter.WriteArrayStart("targets");
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteValue("name", "default");
			    JsonWriter.WriteArrayStart("applyToProducts");
			    JsonWriter.WriteValue("default");
			    JsonWriter.WriteArrayEnd();
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteArrayEnd();
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteArrayEnd();

			    JsonWriter.WriteObjectEnd();
		    }
	    }
	    
	    public void ReplaceProjectOHPackageJson(string IntermediateOpenHarmonyPath)
	    {
		    string ProjectOHPackage = Path.Combine(IntermediateOpenHarmonyPath, "oh-package.json5");
		    ProcessPluginNode("arm64-v8a", "OHProjectPackageJsonUpdate", "");
		    string AppName;
		    EngineIni.GetString("/Script/OpenHarmonyRuntimeSettings.OpenHarmonyRuntimeSettings", "AppName", out AppName);
		    using (JsonWriter JsonWriter = new JsonWriter(ProjectOHPackage))
		    {
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteValue("modelVersion", "5.0.0");
			    JsonWriter.WriteValue("description", "Please describe the basic information.");
			    JsonWriter.WriteObjectStart("dependencies");
			    //工程的依赖写在这里
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectStart("overrides");
			    //工程依赖替换为另一个版本的写在这里
			    if (GlobalContext.StringPairUpdates.TryGetValue("PackageOverrides", out var packageOverrides))
			    {
				    foreach (var packageOverridePair in packageOverrides)
				    {
					    JsonWriter.WriteValue(packageOverridePair.Key, packageOverridePair.Value);
				    }
			    }
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectStart("devDependencies");
			    JsonWriter.WriteValue("@ohos/hypium", "1.0.18");
			    JsonWriter.WriteValue("@ohos/hamock", "1.0.0");
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectEnd();
		    }
	    }
	    
	    public void ReplaceEntryOHPackageJson(string IntermediateOpenHarmonyPath)
	    {
		    string EntryOHPackage = Path.Combine(IntermediateOpenHarmonyPath, "entry", "oh-package.json5");
		    ProcessPluginNode("arm64-v8a", "OHEntryPackageJsonUpdate", "");
		    using (JsonWriter JsonWriter = new JsonWriter(EntryOHPackage))
		    {
			    JsonWriter.WriteObjectStart();
			    JsonWriter.WriteValue("name", "entry");
			    JsonWriter.WriteValue("version", "1.0.0");
			    JsonWriter.WriteValue("description", "Please describe the basic information.");
			    JsonWriter.WriteValue("main", "");
			    JsonWriter.WriteValue("author", "");
			    JsonWriter.WriteValue("license", "");
			    JsonWriter.WriteObjectStart("dependencies");
			    if (GlobalContext.StringPairUpdates.ContainsKey("PackageDependencies"))
			    {
				    foreach (var PackageDependencyPair in GlobalContext.StringPairUpdates["PackageDependencies"])
				    {
					    JsonWriter.WriteValue(PackageDependencyPair.Key, PackageDependencyPair.Value);
				    }
			    }
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectStart("devDependencies");
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectStart("dynamicDependencies");
			    JsonWriter.WriteObjectEnd();
			    JsonWriter.WriteObjectEnd();
		    }
	    }
	    
	    public void ReplaceEntryBuildProfileJson(string IntermediateOpenHarmonyPath)
		{
			string EntryBuildProfile = Path.Combine(IntermediateOpenHarmonyPath, "entry", "build-profile.json5");
			ProcessPluginNode("arm64-v8a", "OHEntryBuildProfileJsonUpdate", "");
			using (JsonWriter JsonWriter = new JsonWriter(EntryBuildProfile))
			{
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("apiType", "stageMode");
				JsonWriter.WriteObjectStart("buildOption");
				JsonWriter.WriteObjectStart("arkOptions");
				JsonWriter.WriteObjectStart("runtimeOnly");
				JsonWriter.WriteArrayStart("packages");
				//entry目录下的build-profile.json5添加Package依赖
				if (GlobalContext.StringValueUpdates.ContainsKey("HarPackageNames"))
				{
					foreach (var PackageName in GlobalContext.StringValueUpdates["HarPackageNames"])
					{
						JsonWriter.WriteValue(PackageName.ToLower());
					}
				}
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteObjectStart("externalNativeOptions");
				JsonWriter.WriteValue("path", "./src/main/cpp/CMakeLists.txt");
				JsonWriter.WriteValue("arguments", "");
				JsonWriter.WriteValue("cppFlags", "");
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteObjectStart("nativeLib");
				JsonWriter.WriteObjectStart("debugSymbol");
				JsonWriter.WriteValue("strip", true);
				JsonWriter.WriteArrayStart("exclude");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart("sourceOption");
				JsonWriter.WriteArrayStart("workers");
				JsonWriter.WriteValue("./src/main/ets/workers/UeWorker.ets");
				JsonWriter.WriteValue("./src/main/ets/workers/AkiWorker.ets");
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectEnd();
				
				JsonWriter.WriteArrayStart("targets");
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "default");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteObjectStart();
				JsonWriter.WriteValue("name", "ohosTest");
				JsonWriter.WriteObjectEnd();
				JsonWriter.WriteArrayEnd();
				JsonWriter.WriteObjectEnd();
			}
		}
	    
		public void ReplaceOhpmrc(string IntermediateOpenHarmonyPath)
		{
			string OhpmrcFilePath = Path.Combine(IntermediateOpenHarmonyPath, ".ohpmrc");
			ProcessPluginNode("arm64-v8a", "OhpmrcUpdates", "");
			
			bool isAppend = false;
			StreamWriter Writer = new StreamWriter(OhpmrcFilePath, isAppend, Encoding.UTF8);
			
			Writer.Write("registry=");
			Writer.Write(string.Join(",", GlobalContext.StringValueUpdates["OhpmrcRegistry"]));
			Writer.WriteLine();
			Writer.WriteLine("strict_ssl=true");
			Writer.WriteLine("publish_registry=https://ohpm.openharmony.cn/ohpm/");
			Writer.WriteLine("log_level=info");
			Writer.WriteLine("fetch_timeout=60000");
			Writer.WriteLine("max_concurrent=50");
			Writer.WriteLine("retry_times=1");
			Writer.WriteLine("retry_interval=1000");
			Writer.WriteLine("resolve_conflict=true");
			Writer.WriteLine("resolve_conflict_strict=false");
			Writer.WriteLine("install_all=true");
			Writer.WriteLine("enable_cross_process_lock=true");
			Writer.WriteLine("enforce_dependency_key=false");
			Writer.WriteLine("ensure_dependency_include=false");
			Writer.WriteLine("disallow_nested_package=false");
			Writer.WriteLine("odm_r2_project_root=false");
			Writer.WriteLine("compability_log_level=warn");
			Writer.WriteLine("use_stream_threshold_size=5");
			Writer.WriteLine("lockfile_stable_order=false");
			
			Writer.Close();
		}
	    
	    public void UpdateEntryAbility(string NDKArch, string EngineDir, string UnrealBuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "OpenHarmony", "entry", "src", "main", "ets", "entryability", "EntryAbility.ets.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "entry", "src", "main", "ets", "entryability", "EntryAbility.ets");
			// check for EntryAbility.ets.template override
			SourceFilename = ProcessPluginNode(NDKArch, "gameActivityReplacement", SourceFilename);
			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${entryAbilityImportAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityImportAdditions", "")},
				{ "//$${entryAbilityStaticInitializationAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityStaticInitializationAdditions", "")},
				{ "//$${entryAbilityOnCreateAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityOnCreateAdditions", "")},
				{ "//$${entryAbilityWindowStageActiveAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityWindowStageActiveAdditions", "")},
				{ "//$${entryAbilityWindowStageInactiveAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityWindowStageInactiveAdditions", "")},
				{ "//$${entryAbilityClassAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityClassAdditions", "")},
				{ "//$${entryAbilityExternalClassAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityExternalClassAdditions", "")},
				{ "//$${entryAbilityonDestroyAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonDestroyAdditions", "")},
				{ "//$${entryAbilityonWindowStageCreateAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonWindowStageCreateAdditions", "")},
				{ "//$${entryAbilityonWindowStageDestroyAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonWindowStageDestroyAdditions", "")},
				{ "//$${entryAbilityonForegroundAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonForegroundAdditions", "")},
				{ "//$${entryAbilityonBackgroundAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonBackgroundAdditions", "")},
				{ "//$${entryAbilityonConfigurationUpdateAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityonConfigurationUpdateAdditions", "")},
				{ "//$${entryAbilityWindowStageloadContentAdditions}$$", ProcessPluginNode(NDKArch, "entryAbilityWindowStageloadContentAdditions", "")},
			};
			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : [];
			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}
			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = string.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}
			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("\n==== Writing new EntryAbility.ets file to {0} ====", DestFilename);
				MakeDirectoryIfRequired(DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

	    public void UpdateAkiJSBind(string NDKArch, string EngineDir, string UnrealBuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "OpenHarmony", "entry", "src", "main", "ets", "common", "AkiJSBind.ets.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "entry", "src", "main", "ets", "common", "AkiJSBind.ets");
			// check for AkiJSBind.ets.templat override
			SourceFilename = ProcessPluginNode(NDKArch, "gameActivityReplacement", SourceFilename);
			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${AkiJSBindImportAdditions}$$", ProcessPluginNode(NDKArch, "AkiJSBindImportAdditions", "")},
				{ "//$${AkiJSBindMainThreadAdditions}$$", ProcessPluginNode(NDKArch, "AkiJSBindMainThreadAdditions", "")},
				{ "//$${AkiJSBindUeWorkerAdditions}$$", ProcessPluginNode(NDKArch, "AkiJSBindUeWorkerAdditions", "")}
			};
			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[] TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : [];
			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if(SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}
			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = string.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}
			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("\n==== Writing new AkiJSBind.ets file to {0} ====", DestFilename);
				MakeDirectoryIfRequired(DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}
	    
		#endregion
	}

	/// <summary>
	/// Equivalent of FConfigCacheIni_UPL. Parses ini files.  This version reads ALL sections since ConfigCacheIni_UPL does NOT
	/// </summary>
	class ConfigCacheIni_UPL
	{
		/// <summary>
		/// Exception when parsing ini files
		/// </summary>
		public class IniParsingException : Exception
		{
			public IniParsingException(string Message)
				: base(Message)
			{ }
			public IniParsingException(string Format, params object[] Args)
				: base(String.Format(Format, Args))
			{ }
		}

		/// <summary>
		/// command class for being able to create config caches over and over without needing to read the ini files
		/// </summary>
		public class Command
		{
			public string? TrimmedLine;
		}

		class SectionCommand : Command
		{
			public FileReference? Filename;
			public int LineIndex;
		}

		class KeyValueCommand : Command
		{
			public string? Key;
			public string? Value;
			public ParseAction LastAction;
		}

		// cached ini files
		static Dictionary<string, List<Command>> FileCache = new Dictionary<string, List<Command>>();
		static Dictionary<string, ConfigCacheIni_UPL> IniCache = new Dictionary<string, ConfigCacheIni_UPL>();
		static Dictionary<string, ConfigCacheIni_UPL> BaseIniCache = new Dictionary<string, ConfigCacheIni_UPL>();

		// static creation functions for ini files
		public static ConfigCacheIni_UPL CreateConfigCacheIni_UPL(UnrealTargetPlatform Platform, string BaseIniName, DirectoryReference? ProjectDirectory, ILogger Logger, DirectoryReference? EngineDirectory = null)
		{
			if (EngineDirectory == null)
			{
				EngineDirectory = Unreal.EngineDirectory;
			}

			// cache base ini for use as the seed for the rest
			if (!BaseIniCache.ContainsKey(BaseIniName))
			{
				BaseIniCache.Add(BaseIniName, new ConfigCacheIni_UPL(BuildHostPlatform.Current.Platform, BaseIniName, null, Logger, EngineDirectory, EngineOnly: true));
			}

			// build the new ini and cache it for later re-use
			ConfigCacheIni_UPL BaseCache = BaseIniCache[BaseIniName];
			string Key = GetIniPlatformName(Platform) + BaseIniName + EngineDirectory.FullName + (ProjectDirectory != null ? ProjectDirectory.FullName : "");
			if (!IniCache.ContainsKey(Key))
			{
				IniCache.Add(Key, new ConfigCacheIni_UPL(Platform, BaseIniName, ProjectDirectory, Logger, EngineDirectory, BaseCache: BaseCache));
			}
			return IniCache[Key];
		}

		/// <summary>
		/// List of values (or a single value)
		/// </summary>
		public class IniValues : List<string>
		{
			public IniValues()
			{
			}
			public IniValues(IniValues Other)
				: base(Other)
			{
			}
			public override string ToString()
			{
				return String.Join(",", ToArray());
			}
		}

		/// <summary>
		/// Ini section (map of keys and values)
		/// </summary>
		public class IniSection : Dictionary<string, IniValues>
		{
			public IniSection()
				: base(StringComparer.InvariantCultureIgnoreCase)
			{ }
			public IniSection(IniSection Other)
				: this()
			{
				foreach (KeyValuePair<string, IniValues> Pair in Other)
				{
					Add(Pair.Key, new IniValues(Pair.Value));
				}
			}
			public override string ToString()
			{
				return "IniSection";
			}
		}

		/// <summary>
		/// True if we are loading a hierarchy of config files that should be merged together
		/// </summary>
		bool bIsMergingConfigs;

		/// <summary>
		/// All sections parsed from ini file
		/// </summary>
		Dictionary<string, IniSection> Sections;

		private ConfigCacheIni_UPL()
		{
			Sections = new Dictionary<string, IniSection>(StringComparer.InvariantCultureIgnoreCase);
		}

		/// <summary>
		/// Constructor. Parses a single ini file. No Platform settings, no engine hierarchy. Do not use this with ini files that have hierarchy!
		/// </summary>
		/// <param name="Filename">The ini file to load</param>
		/// <param name="Logger"></param>
		public ConfigCacheIni_UPL(FileReference Filename, ILogger Logger) : this()
		{
			bIsMergingConfigs = false;
			ParseIniFile(Filename, Logger);
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified project.  No Platform settings.
		/// </summary>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Logger">Logger for </param>
		/// <param name="EngineDirectory"></param>
		public ConfigCacheIni_UPL(string BaseIniName, string? ProjectDirectory, ILogger Logger, string? EngineDirectory = null)
			: this(BuildHostPlatform.Current.Platform, BaseIniName, ProjectDirectory, Logger, EngineDirectory)
		{
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified project.  No Platform settings.
		/// </summary>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="EngineDirectory"></param>
		public ConfigCacheIni_UPL(string BaseIniName, DirectoryReference ProjectDirectory, ILogger Logger, DirectoryReference? EngineDirectory = null)
			: this(BuildHostPlatform.Current.Platform, BaseIniName, ProjectDirectory, Logger, EngineDirectory)
		{
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified platform and project.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		/// <param name="Logger"></param>
		/// <param name="EngineDirectory"></param>
		public ConfigCacheIni_UPL(UnrealTargetPlatform Platform, string BaseIniName, string? ProjectDirectory, ILogger Logger, string? EngineDirectory = null)
			: this(Platform, BaseIniName,
				  (ProjectDirectory == null) ? null : new DirectoryReference(ProjectDirectory),
				  Logger,
				  (EngineDirectory == null) ? null : new DirectoryReference(EngineDirectory))
		{
		}

		/// <summary>
		/// Constructor. Parses ini hierarchy for the specified platform and project.
		/// </summary>
		/// <param name="ProjectDirectory">Project path</param>
		/// <param name="Platform">Target platform</param>
		/// <param name="BaseIniName">Ini name (Engine, Editor, etc)</param>
		/// <param name="Logger"></param>
		/// <param name="EngineDirectory"></param>
		/// <param name="EngineOnly"></param>
		/// <param name="BaseCache"></param>
		public ConfigCacheIni_UPL(UnrealTargetPlatform Platform, string BaseIniName, DirectoryReference? ProjectDirectory, ILogger Logger, DirectoryReference? EngineDirectory = null, bool EngineOnly = false, ConfigCacheIni_UPL? BaseCache = null) : this()
		{
			bIsMergingConfigs = true;
			if (EngineDirectory == null)
			{
				EngineDirectory = Unreal.EngineDirectory;
			}

			if (BaseCache != null)
			{
				foreach (KeyValuePair<string, IniSection> Pair in BaseCache.Sections)
				{
					Sections.Add(Pair.Key, new IniSection(Pair.Value));
				}
			}
			if (EngineOnly)
			{
				foreach (FileReference IniFileName in EnumerateEngineIniFileNames(EngineDirectory, BaseIniName))
				{
					if (FileReference.Exists(IniFileName))
					{
						ParseIniFile(IniFileName, Logger);
					}
				}
			}
			else
			{
				foreach (FileReference IniFileName in EnumerateCrossPlatformIniFileNames(ProjectDirectory, EngineDirectory, Platform, BaseIniName, BaseCache != null))
				{
					if (FileReference.Exists(IniFileName))
					{
						ParseIniFile(IniFileName, Logger);
					}
				}
			}
		}

		/// <summary>
		/// Finds a section in INI
		/// </summary>
		/// <param name="SectionName"></param>
		/// <returns>Found section or null</returns>
		public IniSection? FindSection(string SectionName)
		{
			IniSection? Section;
			Sections.TryGetValue(SectionName, out Section);
			return Section;
		}

		/// <summary>
		/// Finds values associated with the specified key (does not copy the list)
		/// </summary>
		private bool GetList(string SectionName, string Key, [NotNullWhen(true)] out IniValues? Value)
		{
			Value = null;

			IniSection? Section = FindSection(SectionName);
			if (Section == null)
			{
				return false;
			}

			if (Section.TryGetValue(Key, out Value) && Value != null)
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Gets all values associated with the specified key
		/// </summary>
		/// <param name="SectionName">Section where the key is located</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Copy of the list containing all values associated with the specified key</param>
		/// <returns>True if the key exists</returns>
		public bool GetArray(string SectionName, string Key, [NotNullWhen(true)] out List<string>? Value)
		{
			Value = null;
			if (GetList(SectionName, Key, out IniValues? ValueList) && ValueList != null)
			{
				Value = new List<string>(ValueList);
				return true;
			}
			return false;
		}

		/// <summary>
		/// Gets a single string value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetString(string SectionName, string Key, out string Value)
		{
			Value = String.Empty;
			IniValues? ValueList;
			bool Result = GetList(SectionName, Key, out ValueList);
			if (Result && ValueList != null && ValueList.Count > 0)
			{
				Value = ValueList[0];
				Result = true;
			}
			else
			{
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Gets a single bool value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetBool(string SectionName, string Key, out bool Value)
		{
			Value = false;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				// C# Boolean type expects "False" or "True" but since we're not case sensitive, we need to suppor that manually
				if (String.Equals(TextValue, "true", StringComparison.CurrentCultureIgnoreCase) || String.Equals(TextValue, "1"))
				{
					Value = true;
				}
				else if (String.Equals(TextValue, "false", StringComparison.CurrentCultureIgnoreCase) || String.Equals(TextValue, "0"))
				{
					Value = false;
				}
				else
				{
					// Failed to parse
					Result = false;
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets a single Int32 value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetInt32(string SectionName, string Key, out int Value)
		{
			Value = 0;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Int32.TryParse(TextValue, out Value);
			}
			return Result;
		}

		/// <summary>
		/// Gets a single GUID value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetGUID(string SectionName, string Key, out Guid Value)
		{
			Value = Guid.Empty;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				string HexString = "";
				if (TextValue.Contains("A=") && TextValue.Contains("B=") && TextValue.Contains("C=") && TextValue.Contains("D="))
				{
					char[] Separators = new char[] { '(', ')', '=', ',', ' ', 'A', 'B', 'C', 'D' };
					string[] ComponentValues = TextValue.Split(Separators, StringSplitOptions.RemoveEmptyEntries);
					if (ComponentValues.Length == 4)
					{
						for (int ComponentIndex = 0; ComponentIndex < 4; ComponentIndex++)
						{
							int IntegerValue;
							Result &= Int32.TryParse(ComponentValues[ComponentIndex], out IntegerValue);
							HexString += IntegerValue.ToString("X8");
						}
					}
				}
				else
				{
					HexString = TextValue;
				}

				try
				{
					Value = Guid.ParseExact(HexString, "N");
					Result = true;
				}
				catch (Exception)
				{
					Result = false;
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets a single float value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetSingle(string SectionName, string Key, out float Value)
		{
			Value = 0.0f;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Single.TryParse(TextValue, out Value);
			}
			return Result;
		}

		/// <summary>
		/// Gets a single double value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="Key">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetDouble(string SectionName, string Key, out double Value)
		{
			Value = 0.0;
			string TextValue;
			bool Result = GetString(SectionName, Key, out TextValue);
			if (Result)
			{
				Result = Double.TryParse(TextValue, out Value);
			}
			return Result;
		}

		private static bool ExtractPath(string Source, out string Path)
		{
			int start = Source.IndexOf('"');
			int end = Source.LastIndexOf('"');
			if (start != 1 && end != -1 && start < end)
			{
				++start;
				Path = Source.Substring(start, end - start);
				return true;
			}
			else
			{
				Path = "";
			}

			return false;
		}

		public bool GetPath(string SectionName, string Key, out string Value)
		{
			string temp;
			if (GetString(SectionName, Key, out temp))
			{
				return ExtractPath(temp, out Value);
			}
			else
			{
				Value = "";
			}

			return false;
		}

		/// <summary>
		/// List of actions that can be performed on a single line from ini file
		/// </summary>
		enum ParseAction
		{
			None,
			New,
			Add,
			Remove
		}

		/// <summary>
		/// Checks what action should be performed on a single line from ini file
		/// </summary>
		private ParseAction GetActionForLine(ref string Line)
		{
			if (String.IsNullOrEmpty(Line) || Line.StartsWith(";") || Line.StartsWith("//"))
			{
				return ParseAction.None;
			}
			else if (Line.StartsWith("-"))
			{
				Line = Line.Substring(1).TrimStart();
				return ParseAction.Remove;
			}
			else if (Line.StartsWith("+"))
			{
				Line = Line.Substring(1).TrimStart();
				return ParseAction.Add;
			}
			else
			{
				// We use Add rather than New when we're not merging config files together in order 
				// to mimic the behavior of the C++ config cache when loading a single file
				return (bIsMergingConfigs) ? ParseAction.New : ParseAction.Add;
			}
		}

		/// <summary>
		/// Loads and parses ini file.
		/// </summary>
		public void ParseIniFile(FileReference Filename, ILogger Logger)
		{
			string[]? IniLines = null;
			List<Command>? Commands = null;
			if (!FileCache.ContainsKey(Filename.FullName))
			{
				try
				{
					IniLines = File.ReadAllLines(Filename.FullName);
					Commands = new List<Command>();
					FileCache.Add(Filename.FullName, Commands);
				}
				catch (Exception ex)
				{
					Logger.LogInformation("Error reading ini file: {Filename} Exception: {Ex}", Filename, ex.Message);
				}
			}
			else
			{
				Commands = FileCache[Filename.FullName];
			}
			if (IniLines != null && Commands != null)
			{
				IniSection? CurrentSection = null;

				// Line Index for exceptions
				int LineIndex = 1;
				bool bMultiLine = false;
				string SingleValue = "";
				string Key = "";
				ParseAction LastAction = ParseAction.None;

				// Parse each line
				foreach (string Line in IniLines)
				{
					string TrimmedLine = Line.Trim();
					// Multiline value support
					bool bWasMultiLine = bMultiLine;
					bMultiLine = TrimmedLine.EndsWith("\\");
					if (bMultiLine)
					{
						TrimmedLine = TrimmedLine.Substring(0, TrimmedLine.Length - 1).TrimEnd();
					}
					if (!bWasMultiLine)
					{
						if (TrimmedLine.StartsWith("["))
						{
							CurrentSection = FindOrAddSection(TrimmedLine, Filename, LineIndex);
							LastAction = ParseAction.None;
							if (CurrentSection != null)
							{
								SectionCommand Command = new SectionCommand();
								Command.Filename = Filename;
								Command.LineIndex = LineIndex;
								Command.TrimmedLine = TrimmedLine;
								Commands.Add(Command);
							}
						}
						else
						{
							if (LastAction != ParseAction.None)
							{
								throw new IniParsingException("Parsing new key/value pair when the previous one has not yet been processed ({0}, {1}) in {2}, line {3}: {4}", Key, SingleValue, Filename, LineIndex, TrimmedLine);
							}
							// Check if the line is empty or a comment, also remove any +/- markers
							LastAction = GetActionForLine(ref TrimmedLine);
							if (LastAction != ParseAction.None)
							{
								/*								if (CurrentSection == null)
																{
																	throw new IniParsingException("Trying to parse key/value pair that doesn't belong to any section in {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
																}*/
								ParseKeyValuePair(TrimmedLine, Filename, LineIndex, out Key, out SingleValue);
							}
						}
					}
					if (bWasMultiLine)
					{
						SingleValue += TrimmedLine;
					}
					if (!bMultiLine && LastAction != ParseAction.None && CurrentSection != null)
					{
						ProcessKeyValuePair(CurrentSection, Key, SingleValue, LastAction);
						KeyValueCommand Command = new KeyValueCommand();
						Command.Key = Key;
						Command.Value = SingleValue;
						Command.LastAction = LastAction;
						Commands.Add(Command);
						LastAction = ParseAction.None;
						SingleValue = "";
						Key = "";
					}
					else if (CurrentSection == null)
					{
						LastAction = ParseAction.None;
					}
					LineIndex++;
				}
			}
			else if (Commands != null)
			{
				IniSection? CurrentSection = null;

				// run each command
				for (int Idx = 0; Idx < Commands.Count; ++Idx)
				{
					Command Command = Commands[Idx];
					if (Command is SectionCommand SectionCommand)
					{
						CurrentSection = FindOrAddSection(SectionCommand.TrimmedLine!, SectionCommand.Filename!, SectionCommand.LineIndex);
					}
					else if (Command is KeyValueCommand KeyValueCommand)
					{
						ProcessKeyValuePair(CurrentSection!, KeyValueCommand.Key!, KeyValueCommand.Value!, KeyValueCommand.LastAction);
					}
				}
			}
		}

		/// <summary>
		/// Splits a line into key and value
		/// </summary>
		private static void ParseKeyValuePair(string TrimmedLine, FileReference Filename, int LineIndex, out string Key, out string Value)
		{
			int AssignIndex = TrimmedLine.IndexOf('=');
			if (AssignIndex < 0)
			{
				throw new IniParsingException("Failed to find value when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			Key = TrimmedLine.Substring(0, AssignIndex).Trim();
			if (String.IsNullOrEmpty(Key))
			{
				throw new IniParsingException("Empty key when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			Value = TrimmedLine.Substring(AssignIndex + 1).Trim();
			if (Value.StartsWith("\""))
			{
				// Remove quotes
				int QuoteEnd = Value.LastIndexOf('\"');
				if (QuoteEnd == 0)
				{
					throw new IniParsingException("Mismatched quotes when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
				}
				Value = Value.Substring(1, Value.Length - 2);
			}
		}

		/// <summary>
		/// Processes parsed key/value pair
		/// </summary>
		private static void ProcessKeyValuePair(IniSection CurrentSection, string Key, string SingleValue, ParseAction Action)
		{
			switch (Action)
			{
				case ParseAction.New:
					{
						// New/replace
						IniValues? Value;
						if (CurrentSection.TryGetValue(Key, out Value) == false)
						{
							Value = new IniValues();
							CurrentSection.Add(Key, Value);
						}
						Value.Clear();
						Value.Add(SingleValue);
					}
					break;
				case ParseAction.Add:
					{
						IniValues? Value;
						if (CurrentSection.TryGetValue(Key, out Value) == false)
						{
							Value = new IniValues();
							CurrentSection.Add(Key, Value);
						}
						Value.Add(SingleValue);
					}
					break;
				case ParseAction.Remove:
					{
						IniValues? Value;
						if (CurrentSection.TryGetValue(Key, out Value))
						{
							int ExistingIndex = Value.FindIndex(X => (String.Equals(SingleValue, X, StringComparison.CurrentCultureIgnoreCase)));
							if (ExistingIndex >= 0)
							{
								Value.RemoveAt(ExistingIndex);
							}
						}
					}
					break;
			}
		}

		/// <summary>
		/// Finds an existing section or adds a new one
		/// </summary>
		private IniSection FindOrAddSection(string TrimmedLine, FileReference Filename, int LineIndex)
		{
			int SectionEndIndex = TrimmedLine.IndexOf(']');
			if (SectionEndIndex < 1)
			{
				throw new IniParsingException("Mismatched brackets when parsing section name in {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}

			// comment could follow the ] but will just be trimmed out
			string SectionName = TrimmedLine.Substring(1, SectionEndIndex - 1);
			if (String.IsNullOrEmpty(SectionName))
			{
				throw new IniParsingException("Empty section name when parsing {0}, line {1}: {2}", Filename, LineIndex, TrimmedLine);
			}
			{
				IniSection? CurrentSection;
				if (Sections.TryGetValue(SectionName, out CurrentSection) == false)
				{
					CurrentSection = new IniSection();
					Sections.Add(SectionName, CurrentSection);
				}
				return CurrentSection;
			}
		}

		/// <summary>
		/// Returns a list of INI filenames for the engine
		/// </summary>
		private static IEnumerable<FileReference> EnumerateEngineIniFileNames(DirectoryReference EngineDirectory, string BaseIniName)
		{
			// Engine/Config/Base.ini (included in every ini type, required)
			yield return FileReference.Combine(EngineDirectory, "Config", "Base.ini");

			// Engine/Config/Base* ini
			yield return FileReference.Combine(EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

			// Engine/Config/NotForLicensees/Base* ini
			yield return FileReference.Combine(EngineDirectory, "Restricted", "NotForLicensees", "Config", "Base" + BaseIniName + ".ini");
		}

		/// <summary>
		/// Returns a list of INI filenames for the given project
		/// </summary>
		private static IEnumerable<FileReference> EnumerateCrossPlatformIniFileNames(DirectoryReference? ProjectDirectory, DirectoryReference EngineDirectory, UnrealTargetPlatform Platform, string BaseIniName, bool SkipEngine)
		{
			if (!SkipEngine)
			{
				// Engine/Config/Base.ini (included in every ini type, required)
				yield return FileReference.Combine(EngineDirectory, "Config", "Base.ini");

				// Engine/Config/Base* ini
				yield return FileReference.Combine(EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

				// Engine/Config/NotForLicensees/Base* ini
				yield return FileReference.Combine(EngineDirectory, "Restricted", "NotForLicensees", "Config", "Base" + BaseIniName + ".ini");

				// Engine/Config/NoRedist/Base* ini
				yield return FileReference.Combine(EngineDirectory, "Restricted", "NoRedist", "Config", "Base" + BaseIniName + ".ini");
			}

			if (ProjectDirectory != null)
			{
				// Game/Config/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Config", "Default" + BaseIniName + ".ini");

				// Game/Config/NotForLicensees/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Restricted", "NotForLicensees", "Config", "Default" + BaseIniName + ".ini");

				// Game/Config/NoRedist/Default* ini
				yield return FileReference.Combine(ProjectDirectory, "Restricted", "NoRedist", "Config", "Default" + BaseIniName + ".ini");
			}

			string PlatformName = GetIniPlatformName(Platform);
			// Engine/Config/Platform/Platform* ini
			yield return FileReference.Combine(EngineDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");

			if (ProjectDirectory != null)
			{
				// Game/Config/Platform/Platform* ini
				yield return FileReference.Combine(ProjectDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");
			}

			DirectoryReference? UserSettingsFolder = Unreal.UserSettingDirectory; // Match FPlatformProcess::UserSettingsDir()
			DirectoryReference? PersonalFolder = Unreal.UserDirectory; // Match FPlatformProcess::UserDir()

			if (UserSettingsFolder != null)
			{
				// <AppData>/Unreal/EngineConfig/User* ini
				yield return FileReference.Combine(UserSettingsFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			if (PersonalFolder != null)
			{
				// <Documents>/Unreal/EngineConfig/User* ini
				yield return FileReference.Combine(PersonalFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			// Game/Config/User* ini
			if (ProjectDirectory != null)
			{
				yield return FileReference.Combine(ProjectDirectory, "Config", "User" + BaseIniName + ".ini");
			}
		}

		/// <summary>
		/// Returns the platform name to use as part of platform-specific config files
		/// </summary>
		private static string GetIniPlatformName(UnrealTargetPlatform TargetPlatform)
		{
			if (TargetPlatform == UnrealTargetPlatform.Win64)
			{
				return "Windows";
			}
			else
			{
				return TargetPlatform.ToString();
			}
		}
	}
}
