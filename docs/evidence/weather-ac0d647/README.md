# Weather evidence, feat/next-candidate-16c @ac0d647

Windows PC (RTX 3070 Ti), Development package with interior, 120 s runs `-Weather=Overcast -SkipTitle`, 960x540 window, still at 60 s (cropped, shop sign blurred, metadata removed).

## -DriveClouds=1 (overcast-driveclouds1.jpg)

```text
volumetric_clouds=1
cloud_dome=0
cloud_dome_when_overcast=0
cloud_shadow_map=0
cloud_shadow_light_function=loaded
cloud_mode_applied=volumetric
cloud_dome_component=no (not used by this profile)
drive_cloud_coverage=1
cloud_coverage_params=Cloud_GlobalCoverage (-0.20) in /Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst (fallback)
```

## -DriveClouds=0 (overcast-driveclouds0.jpg)

```text
volumetric_clouds=1
cloud_dome=0
cloud_dome_when_overcast=0
cloud_shadow_map=0
cloud_shadow_light_function=loaded
cloud_mode_applied=volumetric
cloud_dome_component=no (not used by this profile)
drive_cloud_coverage=0
cloud_coverage_params=Cloud_GlobalCoverage (-0.20) in /Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst (fallback)
```

'Rain mesh or material missing' in the logs: absent in both runs.
