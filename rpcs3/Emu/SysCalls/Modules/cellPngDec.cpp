#include "stdafx.h"
#include "Emu/SysCalls/SysCalls.h"
#include "Emu/SysCalls/SC_FUNC.h"

#include "stblib/stb_image.h"

void cellPngDec_init();
Module cellPngDec(0x0018, cellPngDec_init);

//Return Codes
enum
{
	CELL_PNGDEC_ERROR_HEADER 			= 0x80611201,
	CELL_PNGDEC_ERROR_STREAM_FORMAT 	= 0x80611202,
	CELL_PNGDEC_ERROR_ARG 				= 0x80611203,
	CELL_PNGDEC_ERROR_SEQ 				= 0x80611204,
	CELL_PNGDEC_ERROR_BUSY 				= 0x80611205,
	CELL_PNGDEC_ERROR_FATAL 			= 0x80611206,
	CELL_PNGDEC_ERROR_OPEN_FILE 		= 0x80611207,
	CELL_PNGDEC_ERROR_SPU_UNSUPPORT 	= 0x80611208,
	CELL_PNGDEC_ERROR_SPU_ERROR 		= 0x80611209,
	CELL_PNGDEC_ERROR_CB_PARAM 			= 0x8061120a,
};

enum CellPngDecColorSpace
{
	CELL_PNGDEC_GRAYSCALE			= 1,
	CELL_PNGDEC_RGB					= 2,
	CELL_PNGDEC_PALETTE				= 4,
	CELL_PNGDEC_GRAYSCALE_ALPHA		= 9,
	CELL_PNGDEC_RGBA				= 10,
	CELL_PNGDEC_ARGB				= 20,
};

struct CellPngDecInfo
{
    u32 imageWidth;
    u32 imageHeight;
    u32 numComponents;
    u32 colorSpace;			// CellPngDecColorSpace
    u32 bitDepth;
	u32 interlaceMethod;	// CellPngDecInterlaceMode
	u32 chunkInformation;
};

struct CellPngDecSrc
{
    u32 srcSelect;			// CellPngDecStreamSrcSel
    u32 fileName;			// const char*
    u64 fileOffset;			// int64_t
    u32 fileSize;
    u32 streamPtr;
    u32 streamSize;
    u32 spuThreadEnable;	// CellPngDecSpuThreadEna
};

struct CellPngDecInParam
{
	u32 *commandPtr;
	u32 outputMode;			// CellPngDecOutputMode
	u32 outputColorSpace;	// CellPngDecColorSpace
	u32 outputBitDepth;
	u32 outputPackFlag;		// CellPngDecPackFlag
	u32 outputAlphaSelect;	// CellPngDecAlphaSelect
	u32 outputColorAlpha;
};

struct CellPngDecOutParam
{
	u64 outputWidthByte;
	u32 outputWidth;
	u32 outputHeight;
	u32 outputComponents;
	u32 outputBitDepth;
	u32 outputMode;			// CellPngDecOutputMode
	u32 outputColorSpace;	// CellPngDecColorSpace
	u32 useMemorySpace;
};

struct CellPngDecSubHandle //Custom struct
{
	u32 fd;
	u64 fileSize;
	CellPngDecInfo info;
	CellPngDecOutParam outParam;
};


int cellPngDecCreate(u32 mainHandle, u32 threadInParam, u32 threadOutParam)
{
	UNIMPLEMENTED_FUNC(cellPngDec);
	return CELL_OK;
}

int cellPngDecDestroy(u32 mainHandle)
{
	UNIMPLEMENTED_FUNC(cellPngDec);
	return CELL_OK;
}

int cellPngDecOpen(u32 mainHandle, mem32_t subHandle, u32 src_addr, u32 openInfo)
{
	//u32 srcSelect       = Memory.Read32(src_addr);
	u32 fileName		  = Memory.Read32(src_addr+4);
	//u64 fileOffset      = Memory.Read32(src_addr+8);
	//u32 fileSize        = Memory.Read32(src_addr+12);
	//u32 streamPtr       = Memory.Read32(src_addr+16);
	//u32 streamSize      = Memory.Read32(src_addr+20);
	//u32 spuThreadEnable = Memory.Read32(src_addr+24);

	CellPngDecSubHandle *current_subHandle = new CellPngDecSubHandle;

	// Get file descriptor
	u32 fd_addr = Memory.Alloc(sizeof(u32), 1);
	int ret = cellFsOpen(fileName, 0, fd_addr, NULL, 0);
	current_subHandle->fd = Memory.Read32(fd_addr);
	Memory.Free(fd_addr);
	if(ret != 0) return CELL_PNGDEC_ERROR_OPEN_FILE;

	// Get size of file
	u32 sb_addr = Memory.Alloc(52,1);					// Alloc a CellFsStat struct
	cellFsFstat(current_subHandle->fd, sb_addr);
	current_subHandle->fileSize = Memory.Read64(sb_addr+36);	// Get CellFsStat.st_size
	Memory.Free(sb_addr);

	// From now, every u32 subHandle argument is a pointer to a CellPngDecSubHandle struct.
	subHandle += (u32)current_subHandle;

	return CELL_OK;
}

int cellPngDecClose(u32 mainHandle, u32 subHandle)
{
	cellFsClose( ((CellPngDecSubHandle*)subHandle)->fd );
	delete (CellPngDecSubHandle*)subHandle;

	return CELL_OK;
}

int cellPngDecReadHeader(u32 mainHandle, u32 subHandle, mem_class_t info)
{
	const u32& fd = ((CellPngDecSubHandle*)subHandle)->fd;
	const u64& fileSize = ((CellPngDecSubHandle*)subHandle)->fileSize;
	CellPngDecInfo& current_info = ((CellPngDecSubHandle*)subHandle)->info;

	//Check size of file
	if(fileSize < 29) return CELL_PNGDEC_ERROR_HEADER;	// Error: The file is smaller than the length of a PNG header
	
	//Write the header to buffer
	u32 buffer = Memory.Alloc(34,1);					// Alloc buffer for PNG header
	u32 nread = Memory.Alloc(8,1);
	u32 pos_addr = Memory.Alloc(8,1);
	cellFsLseek(fd, 0, 0, pos_addr);
	cellFsRead(fd, buffer, 34, nread);
	Memory.Free(nread);
	Memory.Free(pos_addr);

	if (Memory.Read32(buffer) != 0x89504E47 ||
		Memory.Read32(buffer+4) != 0x0D0A1A0A ||  // Error: The first 8 bytes are not a valid PNG signature
		Memory.Read32(buffer+12) != 0x49484452)   // Error: The PNG file does not start with an IHDR chunk
	{
		Memory.Free(buffer);
		return CELL_PNGDEC_ERROR_HEADER; 
	}

	current_info.imageWidth       = Memory.Read32(buffer+16);
	current_info.imageHeight      = Memory.Read32(buffer+20);						
	switch (Memory.Read8(buffer+25))
	{
	case 0:  current_info.colorSpace = CELL_PNGDEC_GRAYSCALE;		current_info.numComponents = 1; break;
	case 2:  current_info.colorSpace = CELL_PNGDEC_RGB;				current_info.numComponents = 3; break;
	case 3:  current_info.colorSpace = CELL_PNGDEC_PALETTE;			current_info.numComponents = 1; break;
	case 4:  current_info.colorSpace = CELL_PNGDEC_GRAYSCALE_ALPHA; current_info.numComponents = 2; break;
	case 6:	 current_info.colorSpace = CELL_PNGDEC_RGBA;			current_info.numComponents = 4; break;
	default: return CELL_PNGDEC_ERROR_HEADER; // Not supported color type
	}
	current_info.bitDepth         = Memory.Read8(buffer+24);
	current_info.interlaceMethod  = Memory.Read8(buffer+28);
	current_info.chunkInformation = 0;							// Unimplemented

	info += current_info.imageWidth;
	info += current_info.imageHeight;
	info += current_info.numComponents;
	info += current_info.colorSpace;
	info += current_info.bitDepth;
	info += current_info.interlaceMethod;
	info += current_info.chunkInformation;
	Memory.Free(buffer);
	
	return CELL_OK;
}

int cellPngDecDecodeData(u32 mainHandle, u32 subHandle, mem8_ptr_t data, u32 dataCtrlParam_addr, mem_class_t dataOutInfo)
{
	const u32& fd = ((CellPngDecSubHandle*)subHandle)->fd;
	const u64& fileSize = ((CellPngDecSubHandle*)subHandle)->fileSize;
	const CellPngDecOutParam& current_outParam = ((CellPngDecSubHandle*)subHandle)->outParam;

	//Copy the PNG file to a buffer
	u32 buffer = Memory.Alloc(fileSize,1);
	u32 nread = Memory.Alloc(8,1);
	u32 pos_addr = Memory.Alloc(sizeof(u64),1);
	cellFsLseek(fd, 0, 0, pos_addr);
	cellFsRead(fd, buffer, fileSize, nread);
	Memory.Free(nread);
	Memory.Free(pos_addr);

	//Decode PNG file. (TODO: Is there any faster alternative? Can we do it without external libraries?)
	int width, height, actual_components;
	unsigned char *png = (unsigned char*)Memory.VirtualToRealAddr(buffer);
	unsigned char *image = stbi_load_from_memory(png, fileSize, &width, &height, &actual_components, 4);
	Memory.Free(buffer);
	if (!image)	return CELL_PNGDEC_ERROR_STREAM_FORMAT;

	u32 image_size = width * height * 4;
	if (current_outParam.outputColorSpace == CELL_PNGDEC_RGBA){
		for(u32 i = 0; i < image_size; i+=4){
			data += image[i+0];
			data += image[i+1];
			data += image[i+2];
			data += image[i+3];
		}
	}
	else if (current_outParam.outputColorSpace == CELL_PNGDEC_ARGB){
		for(u32 i = 0; i < image_size; i+=4){
			data += image[i+3];
			data += image[i+0];
			data += image[i+1];
			data += image[i+2];
		}
	}
	delete[] image;

	return CELL_OK;
}

int cellPngDecSetParameter(u32 mainHandle, u32 subHandle, u32 inParam_addr, mem_class_t outParam)
{
	CellPngDecInfo& current_info = ((CellPngDecSubHandle*)subHandle)->info;
	CellPngDecOutParam& current_outParam = ((CellPngDecSubHandle*)subHandle)->outParam;

	current_outParam.outputWidthByte	= (current_info.imageWidth * current_info.numComponents * current_info.bitDepth)/8;
	current_outParam.outputWidth		= current_info.imageWidth;
	current_outParam.outputHeight		= current_info.imageHeight;
	current_outParam.outputColorSpace	= Memory.Read32(inParam_addr+8);
	switch (current_outParam.outputColorSpace)
	{
	case CELL_PNGDEC_GRAYSCALE:			current_outParam.outputComponents = 1; break;
	case CELL_PNGDEC_GRAYSCALE_ALPHA:	current_outParam.outputComponents = 2; break;
	case CELL_PNGDEC_PALETTE:			current_outParam.outputComponents = 1; break;
	case CELL_PNGDEC_RGB:				current_outParam.outputComponents = 3; break;
	case CELL_PNGDEC_RGBA:				current_outParam.outputComponents = 4; break;
	case CELL_PNGDEC_ARGB:				current_outParam.outputComponents = 4; break;
	default: return CELL_PNGDEC_ERROR_ARG;		// Not supported color space
	}
	current_outParam.outputBitDepth		= Memory.Read32(inParam_addr+12);
	current_outParam.outputMode			= Memory.Read32(inParam_addr+4); 
	current_outParam.useMemorySpace		= 0;	// Unimplemented

	outParam += current_outParam.outputWidthByte;
	outParam += current_outParam.outputWidth;
	outParam += current_outParam.outputHeight;
	outParam += current_outParam.outputComponents;
	outParam += current_outParam.outputBitDepth;
	outParam += current_outParam.outputMode;
	outParam += current_outParam.outputColorSpace;
	outParam += current_outParam.useMemorySpace;

	return CELL_OK;
}

void cellPngDec_init()
{
	cellPngDec.AddFunc(0x157d30c5, cellPngDecCreate);
	cellPngDec.AddFunc(0x820dae1a, cellPngDecDestroy);
	cellPngDec.AddFunc(0xd2bc5bfd, cellPngDecOpen);
	cellPngDec.AddFunc(0x5b3d1ff1, cellPngDecClose);
	cellPngDec.AddFunc(0x9ccdcc95, cellPngDecReadHeader);
	cellPngDec.AddFunc(0x2310f155, cellPngDecDecodeData);
	cellPngDec.AddFunc(0xe97c9bd4, cellPngDecSetParameter);

	/*cellPngDec.AddFunc(0x48436b2d, cellPngDecExtCreate);
	cellPngDec.AddFunc(0x0c515302, cellPngDecExtOpen);
	cellPngDec.AddFunc(0x8b33f863, cellPngDecExtReadHeader);
	cellPngDec.AddFunc(0x726fc1d0, cellPngDecExtDecodeData);
	cellPngDec.AddFunc(0x9e9d7d42, cellPngDecExtSetParameter);
	cellPngDec.AddFunc(0x7585a275, cellPngDecGetbKGD);
	cellPngDec.AddFunc(0x7a062d26, cellPngDecGetcHRM);
	cellPngDec.AddFunc(0xb153629c, cellPngDecGetgAMA);
	cellPngDec.AddFunc(0xb905ebb7, cellPngDecGethIST);
	cellPngDec.AddFunc(0xf44b6c30, cellPngDecGetiCCP);
	cellPngDec.AddFunc(0x27c921b5, cellPngDecGetoFFs);
	cellPngDec.AddFunc(0xb4fe75e1, cellPngDecGetpCAL);
	cellPngDec.AddFunc(0x3d50016a, cellPngDecGetpHYs);
	cellPngDec.AddFunc(0x30cb334a, cellPngDecGetsBIT);
	cellPngDec.AddFunc(0xc41e1198, cellPngDecGetsCAL);
	cellPngDec.AddFunc(0xa5cdf57e, cellPngDecGetsPLT);
	cellPngDec.AddFunc(0xe4416e82, cellPngDecGetsRGB);
	cellPngDec.AddFunc(0x35a6846c, cellPngDecGettIME);
	cellPngDec.AddFunc(0xb96fb26e, cellPngDecGettRNS);
	cellPngDec.AddFunc(0xe163977f, cellPngDecGetPLTE);
	cellPngDec.AddFunc(0x609ec7d5, cellPngDecUnknownChunks);
	cellPngDec.AddFunc(0xb40ca175, cellPngDecGetTextChunk);*/
}