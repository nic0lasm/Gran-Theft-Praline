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

## Cloud shadow light function, -Weather=Overcast -DriveClouds=1, -ExecCmds (GPU over t >= 20 s of an 80 s run, 960x540)

| Still | -ExecCmds | GPU p50 / p95 ms |
|---|---|---|
| overcast-lf-mean-03-strength0.jpg | gtp.CloudShadowLF.Mean 0.3, gtp.CloudShadowLF.Strength 0 | 7.19 / 8.82 |
| overcast-lf-mean-012-strength0.jpg | gtp.CloudShadowLF.Mean 0.12, gtp.CloudShadowLF.Strength 0 | 7.16 / 8.75 |
| overcast-lf-mean-005-strength0.jpg | gtp.CloudShadowLF.Mean 0.05, gtp.CloudShadowLF.Strength 0 | 7.19 / 8.75 |

Status file in the three runs: cloud_shadow_light_function=loaded, lf_mean=0.850, lf_strength=0.500, lf_coverage=1.000, lf_contrast=3.00; cvar_mean=0.300 / 0.120 / 0.050, cvar_strength=0.000.
| overcast-lf-mean-008-strength0-exposure1.jpg | gtp.CloudShadowLF.Mean 0.08, gtp.CloudShadowLF.Strength 0, r.ExposureOffset 1 (Development package) | 7.28 / 8.84 |
