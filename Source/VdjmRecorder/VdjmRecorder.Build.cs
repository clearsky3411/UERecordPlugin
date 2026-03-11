using System;
using System.IO;
using UnrealBuildTool;

public class VdjmRecorder : ModuleRules
{
    public VdjmRecorder(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "RenderCore", // for recorder
                "RHI",        // for recorder
                "Projects", 	// for recorder
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "SlateRHIRenderer"	//	for recorder
            }
        );
        
        PublicDefinitions.Add(
            $"WITH_VDJM_WIN64_AVENCODER={(Target.Platform == UnrealTargetPlatform.Win64 ? 1 : 0)}"
        );

        if (UnrealTargetPlatform.Win64 == Target.Platform )
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AVCodecsCore",
                    //"AVCodecsCoreRHI"
                }
            );
            PublicDefinitions.Add("WITH_VDJM_WIN64_AVENCODER=1");
        }
        else if (UnrealTargetPlatform.Android == Target.Platform)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Launch",                 // Android JNI 접근용
                    "AndroidRuntimeSettings", // 정말 쓰는 경우만 유지
                }
            );
            //근데 hresult 의 값은 필요로 하거든. 어차피 hresult 그냥 int 값아님? 저거 내가 변경해야겠다.
            // 링크용
            PublicSystemLibraries.Add("mediandk");
            PublicSystemLibraries.Add("android");
            PublicSystemLibraries.Add("log");
            PublicSystemLibraries.Add("EGL");
            PublicSystemLibraries.Add("GLESv3");

            // 인텔리센스 / 헤더 탐색용
            string NdkRoot = Environment.GetEnvironmentVariable("NDKROOT");
            if (string.IsNullOrEmpty(NdkRoot))
            {
                NdkRoot = Environment.GetEnvironmentVariable("NDK_ROOT");
            }

            if (!string.IsNullOrEmpty(NdkRoot))
            {
                string SysrootInclude = Path.Combine(
                    NdkRoot,
                    "toolchains",
                    "llvm",
                    "prebuilt",
                    "windows-x86_64",
                    "sysroot",
                    "usr",
                    "include"
                );

                if (Directory.Exists(SysrootInclude))
                {
                    PublicSystemIncludePaths.Add(SysrootInclude);
                }
            }
        }
    }
}