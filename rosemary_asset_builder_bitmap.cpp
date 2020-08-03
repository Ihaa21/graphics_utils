/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Ihor Szlachtycz $
   $Notice: (C) Copyright 2014 by Dream.Inc, Inc. All Rights Reserved. $
   ======================================================================== */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

internal void TextureLoad(u32 TextureId, char* ImagePath, file_texture* Texture, b32 FlipY)
{
    i32 NumComponents = 0;
    i32 Width = 0;
    i32 Height = 0;
    u32* Data = (u32*)stbi_load(ImagePath, &Width, &Height, &NumComponents, 4);
    Assert(Data);
    
    Texture->AssetTypeId = TextureId;
    Texture->Width = Width;
    Texture->Height = Height;
    Texture->AspectRatio = (f32)Texture->Width / (f32)Texture->Height;
    Texture->Size = Texture->Width*Texture->Height*ASSET_TEXTURE_BYTES_PER_PIXEL;

    // NOTE: Default pixel layout from stb_image is reverse of what we expect in x and y
    u32* SrcPixel = (u32*)Data;
    u32* DstData = (u32*)StatePushArray(u32, Texture->Width*Texture->Height, &Texture->PixelOffset);

    i32 Step = -1;
    u32* DstPixel = DstData + Texture->Height*Texture->Width - 1;
    if (FlipY)
    {
        Step = 1;
        DstPixel = DstData;
    }

    for (u32 Y = 0; Y < Texture->Height; ++Y)
    {
        SrcPixel += Texture->Width;
        u32* SrcRow = SrcPixel - 1;
        for (u32 X = 0; X < Texture->Width; ++X)
        {
            u32 SourcePixel = *SrcRow;
            u8 Red = u8((SourcePixel & 0xFF) >> 0);
            u8 Green = u8((SourcePixel & 0xFF00) >> 8);
            u8 Blue = u8((SourcePixel & 0xFF0000) >> 16);
            u8 Alpha = u8((SourcePixel & 0xFF000000) >> 24);

            v4 Texel = SRGBToLinear(V4(Red, Green, Blue, Alpha));
            Texel = PreMulAlpha(Texel);
            Texel = LinearToSRGB(Texel);
                
            u32 DestColor = (((u32)(Texel.a + 0.5f) << 24) |
                             ((u32)(Texel.b + 0.5f) << 16) |
                             ((u32)(Texel.g + 0.5f) << 8) |
                             ((u32)(Texel.r + 0.5f) << 0));
                
            *DstPixel = DestColor;
            SrcRow -= 1;
            DstPixel += Step;
        }
    }
        
    // NOTE: Delete file from memory
    stbi_image_free(Data);
}

#if 0

#pragma pack(push, 1)
struct bitmap_header
{
    u16 FileType;
    u32 FileSize;
    u16 Reserved1;
    u16 Reserved2;
    u32 BitmapOffset;
    u32 Size;
    i32 Width;
    i32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 SizeOfBitmap;
    i32 HorzResolution;
    i32 VertResolution;
    u32 ColorsUsed;
    u32 ColorsImportant;

    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;
};
#pragma pack(pop)

internal load_request LoadBitmap(u32 BitmapId, u64 Size, u8* Data, file_texture* Texture, b32 FlipY)
{
    load_request Result = {};
    
    bitmap_header* Header = (bitmap_header*)Data;
    Texture->AssetTypeId = BitmapId;
    Texture->Width = Header->Width;
    Texture->Height = Header->Height;
    Texture->AspectRatio = (f32)Header->Width / (f32)Header->Height;
    Texture->Size = Texture->Width*Texture->Height*ASSET_TEXTURE_BYTES_PER_PIXEL;
    Texture->PixelOffset = State.CurrentDataOffset;
    
    // TODO: Make this more general for loading all kinds of bitmap formats
    //Assert(Header->Compression == 0);

    if (Header->BitsPerPixel == 24)
    {
        // NOTE: We use these to load the bitmap to the asset file
        Result.Size = sizeof(u32)*Texture->Width*Texture->Height;
        Result.Data = malloc(Result.Size);

        // TODO: Why do we have this padding?
        u8* SourceDest = (u8*)(Data + Header->BitmapOffset);
        u32 PaddingSize = Texture->Width % 4;

        i32 Step = 1;
        u32* DestPixels = (u32*)Result.Data;
        if (FlipY)
        {
            Step *= -1;
            DestPixels = (u32*)Result.Data + Texture->Height*Texture->Width - 1;
        }
        
        for (i32 Y = 0; Y < Header->Height; ++Y)
        {
            SourceDest += PaddingSize;

            for (i32 X = 0; X < Header->Width; ++X)
            {
                u8 Blue = *(SourceDest);
                u8 Green = *(SourceDest + 1);
                u8 Red = *(SourceDest + 2);
                u8 Alpha = 0xFF;

                v4 Texel = SRGBToLinear(V4(Red, Green, Blue, Alpha));
                Texel = PreMulAlpha(Texel);
                Texel = LinearToSRGB(Texel);
                
                u32 DestColor = (((u32)(Texel.a + 0.5f) << 24) |
                                 ((u32)(Texel.b + 0.5f) << 16) |
                                 ((u32)(Texel.g + 0.5f) << 8) |
                                 ((u32)(Texel.r + 0.5f) << 0));
                
                *DestPixels = DestColor;
                DestPixels += Step;
                SourceDest += 3;
            }
        }

        free(Data);
    }
    else if (Header->BitsPerPixel == 32)
    {
        Result.Size = sizeof(u32)*Texture->Width*Texture->Height;
        Result.Data = malloc(Result.Size);

        u32 RedMask = Header->RedMask;
        u32 GreenMask = Header->GreenMask;
        u32 BlueMask = Header->BlueMask;
        u32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        i32 RedShiftDown = (i32)RedScan.Index;
        i32 GreenShiftDown = (i32)GreenScan.Index;
        i32 BlueShiftDown = (i32)BlueScan.Index;
        i32 AlphaShiftDown = (i32)AlphaScan.Index;

        u8* SourceDest = (u8*)(Data + Header->BitmapOffset);
        u32* DestPixels = (u32*)Result.Data;
        
        for (i32 Y = 0; Y < Header->Height; ++Y)
        {
            for (i32 X = 0; X < Header->Width; ++X)
            {
                u32 Color = *(u32*)SourceDest;
                v4 Texel = V4((Color & RedMask) >> RedShiftDown,
                              (Color & GreenMask) >> GreenShiftDown,
                              (Color & BlueMask) >> BlueShiftDown,
                              (Color & AlphaMask) >> AlphaShiftDown);
                
                Texel = SRGBToLinear(Texel);
                Texel = PreMulAlpha(Texel);
                Texel = LinearToSRGB(Texel);
                
                u32 DestColor = (((u32)(Texel.a + 0.5f) << 24) |
                                 ((u32)(Texel.b + 0.5f) << 16) |
                                 ((u32)(Texel.g + 0.5f) << 8) |
                                 ((u32)(Texel.r + 0.5f) << 0));
                
                *DestPixels++ = DestColor;
                SourceDest += 4;
            }
        }

        free(Data);
    }
    else
    {
        InvalidCodePath;
    }
    
    return(Result);
}

#endif
