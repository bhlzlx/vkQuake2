/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2018-2019 Krzysztof Kondrak

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_misc.c

#include "vk_local.h"

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	r_particletexture = Vk_LoadPic ("***particle***", (byte *)data, 8, 8, it_sprite, 32, NULL);

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = Vk_LoadPic ("***r_notexture***", (byte *)data, 8, 8, it_wall, 32, NULL);
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/* 
================== 
Vk_ScreenShot_f
================== 
*/  
void Vk_ScreenShot_f (void) 
{
	byte		*buffer;
	char		picname[80];
	char		checkname[MAX_OSPATH];
	int			i, temp;
	FILE		*f;
	size_t		buffSize = vid.width * vid.height * 4 + 18;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot", ri.FS_Gamedir());
	Sys_Mkdir(checkname);

	// 
	// find a file name to save it to 
	// 
	strcpy(picname, "quake00.tga");

	for (i = 0; i <= 99; i++)
	{
		picname[5] = i / 10 + '0';
		picname[6] = i % 10 + '0';
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/%s", ri.FS_Gamedir(), picname);
		f = fopen(checkname, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose(f);
	}
	if (i == 100)
	{
		ri.Con_Printf(PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}

	buffer = malloc(buffSize);
	memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width & 255;
	buffer[13] = vid.width >> 8;
	buffer[14] = vid.height & 255;
	buffer[15] = vid.height >> 8;
	buffer[16] = 32;	// pixel size
	buffer[17] = 0x20; // image is upside-down

	QVk_ReadPixels(buffer + 18, vid.width, vid.height);

	// swap rgb to bgr
	if (vk_swapchain.format == VK_FORMAT_R8G8B8A8_UNORM || vk_swapchain.format == VK_FORMAT_R8G8B8A8_SRGB)
	{
		for (i = 18; i < buffSize; i += 4)
		{
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
			buffer[i + 3] = 255; // alpha component
		}
	}
	else
	{
		for (i = 18; i < buffSize; i += 4)
		{
			buffer[i + 3] = 255;
		}
	}

	f = fopen(checkname, "wb");
	fwrite(buffer, 1, buffSize, f);
	fclose(f);

	free(buffer);
	ri.Con_Printf(PRINT_ALL, "Wrote %s\n", picname);
} 

/*
** Vk_Strings_f
*/
void Vk_Strings_f(void)
{
	int i = 0;
	char ver[] = { "vkQuake2 v"VKQUAKE2_VERSION };
	for (i = 0; i < strlen(ver); i++)
		ver[i] += 128;

	uint32_t numDevices = 0;
	int usedDevice = 0;
	VkPhysicalDevice physicalDevices[32]; // make an assumption that nobody has more than 32 GPUs :-)
	VkPhysicalDeviceProperties deviceProperties;
	qboolean isPreferred = false;
	int preferredDevice = (int)vk_device_idx->value;
	uint32_t driverMajor = VK_VERSION_MAJOR(vk_device.properties.driverVersion);
	uint32_t driverMinor = VK_VERSION_MINOR(vk_device.properties.driverVersion);
	uint32_t driverPatch = VK_VERSION_PATCH(vk_device.properties.driverVersion);

	// NVIDIA driver version decoding scheme
	if (vk_device.properties.vendorID == 0x10DE)
	{
		driverMajor = ((uint32_t)(vk_device.properties.driverVersion) >> 22) & 0x3ff;
		driverMinor = ((uint32_t)(vk_device.properties.driverVersion) >> 14) & 0x0ff;

		uint32_t secondary = ((uint32_t)(vk_device.properties.driverVersion) >> 6) & 0x0ff;
		uint32_t tertiary = vk_device.properties.driverVersion & 0x03f;

		driverPatch = (secondary << 8) | tertiary;
	}

	VK_VERIFY(vkEnumeratePhysicalDevices(vk_instance, &numDevices, NULL));
	VK_VERIFY(vkEnumeratePhysicalDevices(vk_instance, &numDevices, physicalDevices));

	if (preferredDevice >= numDevices) preferredDevice = -1;

	ri.Con_Printf(PRINT_ALL, "\n%s\n", ver);
	ri.Con_Printf(PRINT_ALL, "------------------------------------\n");
	ri.Con_Printf(PRINT_ALL, "Vulkan API: %d.%d\n",  VK_VERSION_MAJOR(vk_config.vk_version),
													 VK_VERSION_MINOR(vk_config.vk_version));
	ri.Con_Printf(PRINT_ALL, "Header version: %d\n", VK_HEADER_VERSION);
	ri.Con_Printf(PRINT_ALL, "Devices found:\n");
	for (i = 0; i < numDevices; ++i)
	{
		vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
		isPreferred = (preferredDevice == i) || (preferredDevice < 0 && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
		if (isPreferred) usedDevice = i;
		ri.Con_Printf(PRINT_ALL, "%s#%d: %s\n", isPreferred && numDevices > 1 ? "*  " : "   ", i, deviceProperties.deviceName);
	}
	ri.Con_Printf(PRINT_ALL, "Using device #%d:\n", usedDevice);
	ri.Con_Printf(PRINT_ALL, "   deviceName: %s\n"
							 "   driverVersion: %d.%d.%d (0x%X)\n"
							 "   apiVersion: %d.%d.%d\n"
							 "   deviceID: %d\n"
							 "   vendorID: 0x%X (%s)\n"
							 "   deviceType: %s\n"
							 "   gfx/present/transfer: %d/%d/%d\n",	vk_device.properties.deviceName,
																	driverMajor, driverMinor, driverPatch, vk_device.properties.driverVersion,
																	VK_VERSION_MAJOR(vk_device.properties.apiVersion),
																	VK_VERSION_MINOR(vk_device.properties.apiVersion),
																	VK_VERSION_PATCH(vk_device.properties.apiVersion),
																	vk_device.properties.deviceID,
																	vk_device.properties.vendorID,
																	vk_config.vendor_name,
																	vk_config.device_type,
																	vk_device.gfxFamilyIndex, vk_device.presentFamilyIndex, vk_device.transferFamilyIndex);
	ri.Con_Printf(PRINT_ALL, "Present mode: %s\n", vk_config.present_mode);
	ri.Con_Printf(PRINT_ALL, "Swapchain image format: %d\n", vk_swapchain.format);
	ri.Con_Printf(PRINT_ALL, "Supported present modes: ");
	i = 0;
	while(vk_config.supported_present_modes[i])
	{
		ri.Con_Printf(PRINT_ALL, "%s ", vk_config.supported_present_modes[i++]);
	}
	ri.Con_Printf(PRINT_ALL, "\nEnabled extensions: ");

	i = 0;
	while(vk_config.extensions[i])
	{
		ri.Con_Printf(PRINT_ALL, "%s ", vk_config.extensions[i++]);
	}
	ri.Con_Printf(PRINT_ALL, "\nEnabled layers: ");

	i = 0;
	while(vk_config.layers[i])
	{
		ri.Con_Printf(PRINT_ALL, "%s ", vk_config.layers[i++]);
	}
	ri.Con_Printf(PRINT_ALL, "\n");
}
