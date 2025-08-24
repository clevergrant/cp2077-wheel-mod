# Cyberpunk 2077 G923 Steering Wheel Mod - Project Repository

## 📁 Repository Structure

This repository contains the complete **Cyberpunk 2077 G923 Steering Wheel Mod** with all necessary files for Nexus Mods deployment and Vortex compatibility.

### 🎯 Single Source of Truth

**Main Mod Directory**: `cyberpunk-g923-mod/`

This contains everything needed for the mod:
- **Core Mod Files**: All Lua modules and initialization scripts
- **FOMOD Installer**: `fomod/` directory with guided installation
- **Documentation**: README, changelog, and mod info
- **Nexus-Ready Structure**: Proper file organization for deployment

## 🚀 For End Users

**Download the mod from [Nexus Mods](https://www.nexusmods.com/cyberpunk2077) for the latest stable release.**

### Quick Installation
1. Install [Cyber Engine Tweaks](https://www.nexusmods.com/cyberpunk2077/mods/107) (REQUIRED)
2. Download this mod via Vortex or manually
3. Launch Cyberpunk 2077
4. Open console (~) and type: `g923_status()`
5. Type: `g923_calibrate("auto")`
6. Get in a vehicle and drive!

## 🔧 For Developers

### Repository Contents
- `cyberpunk-g923-mod/` - Complete mod package (deploy this to Nexus)
- Development files and project documentation in root

### Key Features
- **Real DirectInput Integration**: Hardware communication with G923 wheel
- **20+ Console Commands**: Complete configuration system
- **Auto-Calibration**: Machine learning-based setup
- **Vehicle-Specific Handling**: Cars, motorcycles, trucks
- **Force Feedback**: Road texture and collision simulation
- **FOMOD Installer**: Vortex-compatible guided installation

### Contributing
This mod welcomes contributions for:
- DirectInput Windows API enhancements
- Performance optimizations
- Additional hardware support
- Bug fixes and testing

## 📋 Version Info

**Current Version**: 3.0.0 (Production Release)
**Framework**: Cyber Engine Tweaks
**Game**: Cyberpunk 2077 v2.0+
**Hardware**: Logitech G923 Steering Wheel

---

*Transform your Night City driving experience! 🌃🚗*

## Troubleshooting

### Game Won't Start / Script Error

1. **Disable mod**: Rename `cyberpunk-g923-mod` folder to `cyberpunk-g923-mod-DISABLED`
2. **Verify game files** in Steam/GOG/Epic
3. **Update CET** to latest version
4. **Re-enable mod** and test `g923_status()`

### Wheel Not Detected

1. **Check USB connection** (use USB 3.0 port)
2. **Install Logitech G HUB** and calibrate wheel
3. **Test in console**: `g923_hardware("test")`
4. **Try manual calibration**: `g923_calibrate("manual")`

### Poor Performance / Stuttering

1. **Enable performance mode**: `g923_performance("optimization")`
2. **Lower sensitivity**: `g923_sensitivity(1.0)`
3. **Increase deadzones**: `g923_deadzone(10, 15, 10)`
4. **Check frame rate** (60+ FPS recommended)

### No Force Feedback

1. **Enable in mod**: `g923_force_feedback(true)`
2. **Test effects**: `g923_test_effects()`
3. **Check G HUB settings** (enable force feedback)
4. **Try different vehicle** (effects vary by vehicle type)

### Console Commands Not Working

1. **Open and close console** once (press `~` twice)
2. **Try**: `g923_status()` again
3. **Check mod loading**: `print("G923 loaded:", _G.G923Mod ~= nil)`

## Configuration Files

Settings are automatically saved to:

```
Cyberpunk 2077/bin/x64/plugins/cyber_engine_tweaks/mods/cyberpunk-g923-mod/config.json
```

**Backup your config** before updating the mod!

## Compatibility

### Supported Wheels

- **Logitech G923** (Primary support)
- Other DirectInput wheels may work with limited functionality

### Game Versions

- **Cyberpunk 2077 v2.0+** (Required)
- **Phantom Liberty DLC** (Compatible)

### Known Conflicts

- Mods that override vehicle input systems
- Other steering wheel mods
- Some input remapping mods

## Support & Updates

- **Report issues**: Include `g923_status()` output and CET logs
- **Request features**: Describe use case and vehicle type
- **Performance issues**: Include `g923_performance("status")` output

## Version History

**v3.0.0** - Real DirectInput integration, auto-calibration, performance monitoring
**v2.x** - Force feedback system, vehicle-specific handling
**v1.x** - Basic steering wheel support

## Credits

Developed for the Cyberpunk 2077 modding community. Uses Cyber Engine Tweaks framework and DirectInput API for hardware communication.
