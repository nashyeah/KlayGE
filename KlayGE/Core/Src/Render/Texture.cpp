// Texture.cpp
// KlayGE 纹理类 实现文件
// Ver 3.5.0
// 版权所有(C) 龚敏敏, 2005-2007
// Homepage: http://klayge.sourceforge.net
//
// 3.5.0
// 支持有符号格式 (2007.2.12)
//
// 3.3.0
// 支持GR16和ABGR16 (2006.6.7)
//
// 2.4.0
// 初次建立 (2005.3.6)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/RenderView.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KlayGE/Util.hpp>

#include <cstring>
#include <fstream>

#include <KlayGE/Texture.hpp>

namespace
{
	using namespace KlayGE;

#ifdef KLAYGE_PLATFORM_WINDOWS
#pragma pack(push, 1)
#endif

	enum
	{
		// The surface has alpha channel information in the pixel format.
		DDSPF_ALPHAPIXELS = 0x00000001,

		// The FourCC code is valid.
		DDSPF_FOURCC = 0x00000004,

		// The RGB data in the pixel format structure is valid.
		DDSPF_RGB = 0x00000040,

		// Luminance data in the pixel format is valid.
		// Use this flag for luminance-only or luminance+alpha surfaces,
		// the bit depth is then ddpf.dwLuminanceBitCount.
		DDSPF_LUMINANCE = 0x00020000,

		// Bump map dUdV data in the pixel format is valid.
		DDSPF_BUMPDUDV = 0x00080000
	};

	struct DDSPIXELFORMAT
	{
		uint32_t	size;				// size of structure
		uint32_t	flags;				// pixel format flags
		uint32_t	four_cc;			// (FOURCC code)
		uint32_t	rgb_bit_count;		// how many bits per pixel
		uint32_t	r_bit_mask;			// mask for red bit
		uint32_t	g_bit_mask;			// mask for green bits
		uint32_t	b_bit_mask;			// mask for blue bits
		uint32_t	rgb_alpha_bit_mask;	// mask for alpha channels
	};

	enum
	{
		// Indicates a complex surface structure is being described.  A
		// complex surface structure results in the creation of more than
		// one surface.  The additional surfaces are attached to the root
		// surface.  The complex structure can only be destroyed by
		// destroying the root.
		DDSCAPS_COMPLEX		= 0x00000008,

		// Indicates that this surface can be used as a 3D texture.  It does not
		// indicate whether or not the surface is being used for that purpose.
		DDSCAPS_TEXTURE		= 0x00001000,

		// Indicates surface is one level of a mip-map. This surface will
		// be attached to other DDSCAPS_MIPMAP surfaces to form the mip-map.
		// This can be done explicitly, by creating a number of surfaces and
		// attaching them with AddAttachedSurface or by implicitly by CreateSurface.
		// If this bit is set then DDSCAPS_TEXTURE must also be set.
		DDSCAPS_MIPMAP		= 0x00400000,
	};

	enum
	{
		// This flag is used at CreateSurface time to indicate that this set of
		// surfaces is a cubic environment map
		DDSCAPS2_CUBEMAP	= 0x00000200,

		// These flags preform two functions:
		// - At CreateSurface time, they define which of the six cube faces are
		//   required by the application.
		// - After creation, each face in the cubemap will have exactly one of these
		//   bits set.
		DDSCAPS2_CUBEMAP_POSITIVEX	= 0x00000400,
		DDSCAPS2_CUBEMAP_NEGATIVEX	= 0x00000800,
		DDSCAPS2_CUBEMAP_POSITIVEY	= 0x00001000,
		DDSCAPS2_CUBEMAP_NEGATIVEY	= 0x00002000,
		DDSCAPS2_CUBEMAP_POSITIVEZ	= 0x00004000,
		DDSCAPS2_CUBEMAP_NEGATIVEZ	= 0x00008000,

		// Indicates that the surface is a volume.
		// Can be combined with DDSCAPS_MIPMAP to indicate a multi-level volume
		DDSCAPS2_VOLUME		= 0x00200000,
	};

	struct DDSCAPS2
	{
		uint32_t	caps1;			// capabilities of surface wanted
		uint32_t	caps2;
		uint32_t	reserved[2];
	};

	enum
	{
		DDSD_CAPS			= 0x00000001,	// default, dds_caps field is valid.
		DDSD_HEIGHT			= 0x00000002,	// height field is valid.
		DDSD_WIDTH			= 0x00000004,	// width field is valid.
		DDSD_PITCH			= 0x00000008,	// pitch is valid.
		DDSD_PIXELFORMAT	= 0x00001000,	// pixel_format is valid.
		DDSD_MIPMAPCOUNT	= 0x00020000,	// mip_map_count is valid.
		DDSD_LINEARSIZE		= 0x00080000,	// linear_size is valid
		DDSD_DEPTH			= 0x00800000,	// depth is valid
	};

	struct DDSSURFACEDESC2
	{
		uint32_t	size;					// size of the DDSURFACEDESC structure
		uint32_t	flags;					// determines what fields are valid
		uint32_t	height;					// height of surface to be created
		uint32_t	width;					// width of input surface
		union
		{
			int32_t		pitch;				// distance to start of next line (return value only)
			uint32_t	linear_size;		// Formless late-allocated optimized surface size
		};
		uint32_t		depth;				// the depth if this is a volume texture
		uint32_t		mip_map_count;		// number of mip-map levels requestde
		uint32_t		reserved1[11];		// reserved
		DDSPIXELFORMAT	pixel_format;		// pixel format description of the surface
		DDSCAPS2		dds_caps;			// direct draw surface capabilities
		uint32_t		reserved2;
	};

#ifdef KLAYGE_PLATFORM_WINDOWS
#pragma pack(pop)
#endif
}

namespace KlayGE
{
	// 载入DDS格式文件
	TexturePtr LoadTexture(std::string const & tex_name, uint32_t access_hint)
	{
		boost::shared_ptr<std::istream> file(ResLoader::Instance().Load(tex_name));

		uint32_t magic;
		file->read(reinterpret_cast<char*>(&magic), sizeof(magic));
		BOOST_ASSERT((MakeFourCC<'D', 'D', 'S', ' '>::value) == magic);

		DDSSURFACEDESC2 desc;
		file->read(reinterpret_cast<char*>(&desc), sizeof(desc));

		BOOST_ASSERT((desc.flags & DDSD_CAPS) != 0);
		BOOST_ASSERT((desc.flags & DDSD_PIXELFORMAT) != 0);
		BOOST_ASSERT((desc.flags & DDSD_WIDTH) != 0);
		BOOST_ASSERT((desc.flags & DDSD_HEIGHT) != 0);

		if (0 == (desc.flags & DDSD_MIPMAPCOUNT))
		{
			desc.mip_map_count = 1;
		}

		ElementFormat format = EF_ARGB8;
		if ((desc.pixel_format.flags & DDSPF_FOURCC) != 0)
		{
			switch (desc.pixel_format.four_cc)
			{
			case 36:
				format = EF_ABGR16;
				break;

			case 110:
				format = EF_SIGNED_ABGR16;
				break;

			case 111:
				format = EF_R16F;
				break;

			case 112:
				format = EF_GR16F;
				break;

			case 113:
				format = EF_ABGR16F;
				break;

			case 114:
				format = EF_R32F;
				break;

			case 115:
				format = EF_GR32F;
				break;

			case 116:
				format = EF_ABGR32F;
				break;

			case MakeFourCC<'D', 'X', 'T', '1'>::value:
				format = EF_BC1;
				break;

			case MakeFourCC<'D', 'X', 'T', '3'>::value:
				format = EF_BC2;
				break;

			case MakeFourCC<'D', 'X', 'T', '5'>::value:
				format = EF_BC3;
				break;
			}
		}
		else
		{
			if ((desc.pixel_format.flags & DDSPF_RGB) != 0)
			{
				switch (desc.pixel_format.rgb_bit_count)
				{
				case 16:
					if ((0xF800 == desc.pixel_format.r_bit_mask)
						&& (0x7E0 == desc.pixel_format.g_bit_mask)
						&& (0x1F == desc.pixel_format.b_bit_mask))
					{
						format = EF_R5G6B5;
					}
					else
					{
						if ((0xF000 == desc.pixel_format.rgb_alpha_bit_mask)
							&& (0x0F00 == desc.pixel_format.r_bit_mask)
							&& (0x00F0 == desc.pixel_format.g_bit_mask)
							&& (0x000F == desc.pixel_format.b_bit_mask))
						{
							format = EF_ARGB4;
						}
						else
						{
							BOOST_ASSERT(false);
						}
					}
					break;

				case 32:
					if ((0x00FF0000 == desc.pixel_format.r_bit_mask)
						&& (0x0000FF00 == desc.pixel_format.g_bit_mask)
						&& (0x000000FF == desc.pixel_format.b_bit_mask))
					{
						if ((desc.pixel_format.flags & DDSPF_ALPHAPIXELS) != 0)
						{
							format = EF_ARGB8;
						}
						else
						{
							BOOST_ASSERT(false);
						}
					}
					else
					{
						if ((0xC0000000 == desc.pixel_format.rgb_alpha_bit_mask)
							&& (0x3FF00000 == desc.pixel_format.r_bit_mask)
							&& (0x000FFC00 == desc.pixel_format.g_bit_mask)
							&& (0x000003FF == desc.pixel_format.b_bit_mask))
						{
							format = EF_A2BGR10;
						}
						else
						{
							if ((0x00000000 == desc.pixel_format.rgb_alpha_bit_mask)
								&& (0x0000FFFF == desc.pixel_format.r_bit_mask)
								&& (0xFFFF0000 == desc.pixel_format.g_bit_mask)
								&& (0x00000000 == desc.pixel_format.b_bit_mask))
							{
								format = EF_GR16;
							}
							else
							{
								BOOST_ASSERT(false);
							}
						}
					}
					break;
				}
			}
			else
			{
				if ((desc.pixel_format.flags & DDSPF_LUMINANCE) != 0)
				{
					switch (desc.pixel_format.rgb_bit_count)
					{
					case 8:
						if ((desc.pixel_format.flags & DDSPF_ALPHAPIXELS) != 0)
						{
							format = EF_AL4;
						}
						else
						{
							format = EF_L8;
						}
						break;

					case 16:
						if ((desc.pixel_format.flags & DDSPF_ALPHAPIXELS) != 0)
						{
							format = EF_AL8;
						}
						else
						{
							format = EF_L16;
						}
						break;

					default:
						BOOST_ASSERT(false);
						break;
					}
				}
				else
				{
					if ((desc.pixel_format.flags & DDSPF_BUMPDUDV) != 0)
					{
						switch (desc.pixel_format.rgb_bit_count)
						{
						case 16:
							if ((0x000000FF == desc.pixel_format.r_bit_mask)
								&& (0x0000FF00 == desc.pixel_format.g_bit_mask))
							{
								format = EF_SIGNED_GR8;
							}
							else
							{
								if (0x0000FFFF == desc.pixel_format.r_bit_mask)
								{
									format = EF_SIGNED_R16;
								}
								else
								{
									BOOST_ASSERT(false);
								}
							}
							break;

						case 32:
							if ((0x000000FF == desc.pixel_format.r_bit_mask)
								&& (0x0000FF00 == desc.pixel_format.g_bit_mask)
								&& (0x00FF0000 == desc.pixel_format.b_bit_mask))
							{
								format = EF_SIGNED_ABGR8;
							}
							else
							{
								if ((0xC0000000 == desc.pixel_format.rgb_alpha_bit_mask)
									&& (0x3FF00000 == desc.pixel_format.r_bit_mask)
									&& (0x000FFC00 == desc.pixel_format.g_bit_mask)
									&& (0x000003FF == desc.pixel_format.b_bit_mask))
								{
									format = EF_SIGNED_A2BGR10;
								}
								else
								{
									if ((0x00000000 == desc.pixel_format.rgb_alpha_bit_mask)
										&& (0x0000FFFF == desc.pixel_format.r_bit_mask)
										&& (0xFFFF0000 == desc.pixel_format.g_bit_mask)
										&& (0x00000000 == desc.pixel_format.b_bit_mask))
									{
										format = EF_SIGNED_GR16;
									}
									else
									{
										BOOST_ASSERT(false);
									}
								}
							}
							break;

						default:
							BOOST_ASSERT(false);
							break;
						}
					}
					else
					{
						if ((desc.pixel_format.flags & DDSPF_ALPHAPIXELS) != 0)
						{
							format = EF_A8;
						}
						else
						{
							BOOST_ASSERT(false);
						}
					}
				}
			}
		}

		uint32_t main_image_size;
		if ((desc.flags & DDSD_LINEARSIZE) != 0)
		{
			main_image_size = desc.linear_size;
		}
		else
		{
			if ((desc.flags & DDSD_PITCH) != 0)
			{
				main_image_size = desc.pitch * desc.height;
			}
			else
			{
				if ((desc.flags & desc.pixel_format.flags & 0x00000040) != 0)
				{
					main_image_size = desc.width * desc.height * desc.pixel_format.rgb_bit_count / 8;
				}
				else
				{
					main_image_size = desc.width * desc.height * NumFormatBytes(format);
				}
			}
		}

		if (desc.reserved1[0] != 0)
		{
			format = MakeSRGB(format);
		}

		RenderFactory& renderFactory = Context::Instance().RenderFactoryInstance();
		TexturePtr texture_sys_mem;
		{
			if ((desc.dds_caps.caps2 & DDSCAPS2_CUBEMAP) != 0)
			{
				texture_sys_mem = renderFactory.MakeTextureCube(desc.width,
					static_cast<uint16_t>(desc.mip_map_count), format, EAH_CPU_Write, NULL);
			}
			else
			{
				if ((desc.dds_caps.caps2 & DDSCAPS2_VOLUME) != 0)
				{
					texture_sys_mem = renderFactory.MakeTexture3D(desc.width,
						desc.height, desc.depth, static_cast<uint16_t>(desc.mip_map_count), format, EAH_CPU_Write, NULL);
				}
				else
				{
					texture_sys_mem = renderFactory.MakeTexture2D(desc.width,
						desc.height, static_cast<uint16_t>(desc.mip_map_count), format, EAH_CPU_Write, NULL);
				}
			}
		}

		uint32_t format_size = NumFormatBytes(format);
		switch (texture_sys_mem->Type())
		{
		case Texture::TT_1D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t image_size;
					if (IsCompressedFormat(format))
					{
						int block_size;
						if (EF_BC1 == format)
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						image_size = ((texture_sys_mem->Width(level) + 3) / 4) * block_size;
					}
					else
					{
						image_size = main_image_size / (1UL << (level * 2));
					}

					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Write_Only, 0, texture_sys_mem->Width(level));
						file->read(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
						BOOST_ASSERT(file->gcount() == static_cast<int>(image_size));
					}
				}
			}
			break;

		case Texture::TT_2D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t width = texture_sys_mem->Width(level);
					uint32_t height = texture_sys_mem->Height(level);

					if (IsCompressedFormat(format))
					{
						int block_size;
						if (EF_BC1 == format)
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						uint32_t image_size = ((texture_sys_mem->Width(level) + 3) / 4) * ((texture_sys_mem->Height(level) + 3) / 4) * block_size;

						{
							Texture::Mapper mapper(*texture_sys_mem, level, TMA_Write_Only, 0, 0, width, height);
							file->read(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
							BOOST_ASSERT(file->gcount() == static_cast<int>(image_size));
						}
					}
					else
					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Write_Only, 0, 0, width, height);
						char* data = mapper.Pointer<char>();

						for (uint32_t y = 0; y < height; ++ y)
						{
							file->read(data, static_cast<std::streamsize>(width * format_size));
							if ((desc.flags & DDSD_PITCH)
								&& (width * format_size != static_cast<uint32_t>(desc.pitch)))
							{
								file->seekg(desc.pitch - width * format_size, std::ios_base::cur);
							}
							data += mapper.RowPitch();
						}
					}
				}
			}
			break;

		case Texture::TT_3D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t width = texture_sys_mem->Width(level);
					uint32_t height = texture_sys_mem->Height(level);
					uint32_t depth = texture_sys_mem->Depth(level);

					if (IsCompressedFormat(format))
					{
						int block_size;
						if (EF_BC1 == format)
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						uint32_t image_size = ((width + 3) / 4) * ((height + 3) / 4) * depth * block_size;

						{
							Texture::Mapper mapper(*texture_sys_mem, level, TMA_Write_Only, 0, 0, 0, width, height, depth);
							file->read(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
							BOOST_ASSERT(file->gcount() == static_cast<int>(image_size));
						}
					}
					else
					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Write_Only, 0, 0, 0, width, height, depth);
						char* data = mapper.Pointer<char>();

						for (uint32_t z = 0; z < depth; ++ z)
						{
							for (uint32_t y = 0; y < height; ++ y)
							{
								file->read(data, static_cast<std::streamsize>(width * format_size));
								if ((desc.flags & DDSD_PITCH)
									&& (width * format_size != static_cast<uint32_t>(desc.pitch)))
								{
									file->seekg(desc.pitch - width * format_size, std::ios_base::cur);
								}
								data += mapper.RowPitch();
							}

							data += mapper.SlicePitch() - mapper.RowPitch() * height;
						}
					}
				}
			}
			break;

		case Texture::TT_Cube:
			{
				for (uint32_t face = Texture::CF_Positive_X; face <= Texture::CF_Negative_Z; ++ face)
				{
					for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
					{
						uint32_t width = texture_sys_mem->Width(level);
						uint32_t height = texture_sys_mem->Height(level);

						if (IsCompressedFormat(format))
						{
							int block_size;
							if (EF_BC1 == format)
							{
								block_size = 8;
							}
							else
							{
								block_size = 16;
							}

							uint32_t image_size = ((width + 3) / 4) * ((height + 3) / 4) * block_size;

							{
								Texture::Mapper mapper(*texture_sys_mem, static_cast<Texture::CubeFaces>(face), level, TMA_Write_Only, 0, 0, width, height);
								file->read(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
								BOOST_ASSERT(file->gcount() == static_cast<int>(image_size));
							}
						}
						else
						{
							Texture::Mapper mapper(*texture_sys_mem, static_cast<Texture::CubeFaces>(face), level, TMA_Write_Only, 0, 0, width, height);
							char* data = mapper.Pointer<char>();

							for (uint32_t y = 0; y < height; ++ y)
							{
								file->read(data, static_cast<std::streamsize>(width * format_size));
								if ((desc.flags & DDSD_PITCH)
									&& (width * format_size != static_cast<uint32_t>(desc.pitch)))
								{
									file->seekg(desc.pitch - width * format_size, std::ios_base::cur);
								}
								data += mapper.RowPitch();
							}
						}
					}
				}
			}
			break;
		}

		TexturePtr texture;
		switch (texture_sys_mem->Type())
		{
		case Texture::TT_1D:
			texture = renderFactory.MakeTexture1D(texture_sys_mem->Width(0),
				texture_sys_mem->NumMipMaps(), texture_sys_mem->Format(), access_hint, NULL);
			break;

		case Texture::TT_2D:
			texture = renderFactory.MakeTexture2D(texture_sys_mem->Width(0), texture_sys_mem->Height(0),
				texture_sys_mem->NumMipMaps(), texture_sys_mem->Format(), access_hint, NULL);
			break;

		case Texture::TT_3D:
			texture = renderFactory.MakeTexture3D(texture_sys_mem->Width(0), texture_sys_mem->Height(0), texture_sys_mem->Depth(0),
				texture_sys_mem->NumMipMaps(), texture_sys_mem->Format(), access_hint, NULL);
			break;

		case Texture::TT_Cube:
			texture = renderFactory.MakeTextureCube(texture_sys_mem->Width(0),
				texture_sys_mem->NumMipMaps(), texture_sys_mem->Format(), access_hint, NULL);
			break;

		default:
			BOOST_ASSERT(false);
			break;
		}
		texture_sys_mem->CopyToTexture(*texture);

		return texture;
	}

	// 把纹理保存入DDS文件
	void SaveTexture(TexturePtr texture, std::string const & tex_name)
	{
		RenderFactory& renderFactory = Context::Instance().RenderFactoryInstance();

		TexturePtr texture_sys_mem;
		switch (texture->Type())
		{
		case Texture::TT_1D:
			texture_sys_mem = renderFactory.MakeTexture1D(texture->Width(0),
				texture->NumMipMaps(), texture->Format(), EAH_CPU_Read, NULL);
			break;

		case Texture::TT_2D:
			texture_sys_mem = renderFactory.MakeTexture2D(texture->Width(0), texture->Height(0),
				texture->NumMipMaps(), texture->Format(), EAH_CPU_Read, NULL);
			break;

		case Texture::TT_3D:
			texture_sys_mem = renderFactory.MakeTexture3D(texture->Width(0), texture->Height(0),
				texture->Depth(0), texture->NumMipMaps(), texture->Format(), EAH_CPU_Read, NULL);
			break;

		case Texture::TT_Cube:
			texture_sys_mem = renderFactory.MakeTextureCube(texture->Width(0),
				texture->NumMipMaps(), texture->Format(), EAH_CPU_Read, NULL);
			break;

		default:
			BOOST_ASSERT(false);
			break;
		}
		texture->CopyToTexture(*texture_sys_mem);

		std::ofstream file(tex_name.c_str(), std::ios_base::binary);

		uint32_t magic = MakeFourCC<'D', 'D', 'S', ' '>::value;
		file.write(reinterpret_cast<char*>(&magic), sizeof(magic));

		DDSSURFACEDESC2 desc;
		std::memset(&desc, 0, sizeof(desc));

		desc.size = sizeof(desc);

		desc.flags |= DDSD_CAPS;
		desc.flags |= DDSD_PIXELFORMAT;
		desc.flags |= DDSD_WIDTH;
		desc.flags |= DDSD_HEIGHT;

		desc.width = texture_sys_mem->Width(0);
		desc.height = texture_sys_mem->Height(0);

		if (texture_sys_mem->NumMipMaps() != 0)
		{
			desc.flags |= DDSD_MIPMAPCOUNT;
			desc.mip_map_count = texture_sys_mem->NumMipMaps();
		}

		desc.pixel_format.size = sizeof(desc.pixel_format);

		if (IsSRGB(texture_sys_mem->Format()))
		{
			desc.reserved1[0] = 1;
		}

		if ((EF_ABGR16 == texture_sys_mem->Format())
			|| IsFloatFormat(texture_sys_mem->Format()) || IsCompressedFormat(texture_sys_mem->Format()))
		{
			desc.pixel_format.flags |= DDSPF_FOURCC;

			switch (texture_sys_mem->Format())
			{
			case EF_ABGR16:
				desc.pixel_format.four_cc = 36;
				break;

			case EF_SIGNED_ABGR16:
				desc.pixel_format.four_cc = 110;
				break;

			case EF_R16F:
				desc.pixel_format.four_cc = 111;
				break;

			case EF_GR16F:
				desc.pixel_format.four_cc = 112;
				break;

			case EF_ABGR16F:
				desc.pixel_format.four_cc = 113;
				break;

			case EF_R32F:
				desc.pixel_format.four_cc = 114;
				break;

			case EF_GR32F:
				desc.pixel_format.four_cc = 115;
				break;

			case EF_ABGR32F:
				desc.pixel_format.four_cc = 116;
				break;

			case EF_BC1:
			case EF_BC1_SRGB:
				desc.pixel_format.four_cc = MakeFourCC<'D', 'X', 'T', '1'>::value;
				break;

			case EF_BC2:
			case EF_BC2_SRGB:
				desc.pixel_format.four_cc = MakeFourCC<'D', 'X', 'T', '3'>::value;
				break;

			case EF_BC3:
			case EF_BC3_SRGB:
				desc.pixel_format.four_cc = MakeFourCC<'D', 'X', 'T', '5'>::value;
				break;

			default:
				BOOST_ASSERT(false);
				break;
			}
		}
		else
		{
			switch (texture_sys_mem->Format())
			{
			case EF_R5G6B5:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x0000F800;
				desc.pixel_format.g_bit_mask = 0x000007E0;
				desc.pixel_format.b_bit_mask = 0x0000001F;
				break;

			case EF_ARGB4:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x0000F000;
				desc.pixel_format.r_bit_mask = 0x00000F00;
				desc.pixel_format.g_bit_mask = 0x000000F0;
				desc.pixel_format.b_bit_mask = 0x0000000F;
				break;

			case EF_SIGNED_GR8:
				desc.pixel_format.flags |= DDSPF_BUMPDUDV;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x000000FF;
				desc.pixel_format.g_bit_mask = 0x0000FF00;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_SIGNED_R16:
				desc.pixel_format.flags |= DDSPF_BUMPDUDV;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x0000FFFF;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_ARGB8:
			case EF_ARGB8_SRGB:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0xFF000000;
				desc.pixel_format.r_bit_mask = 0x00FF0000;
				desc.pixel_format.g_bit_mask = 0x0000FF00;
				desc.pixel_format.b_bit_mask = 0x000000FF;
				break;

			case EF_ABGR8:
			case EF_ABGR8_SRGB:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0xFF000000;
				desc.pixel_format.r_bit_mask = 0x000000FF;
				desc.pixel_format.g_bit_mask = 0x0000FF00;
				desc.pixel_format.b_bit_mask = 0x00FF0000;
				break;

			case EF_SIGNED_ABGR8:
				desc.pixel_format.flags |= DDSPF_BUMPDUDV;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0xFF00000;
				desc.pixel_format.r_bit_mask = 0x000000FF;
				desc.pixel_format.g_bit_mask = 0x0000FF00;
				desc.pixel_format.b_bit_mask = 0x00FF0000;
				break;

			case EF_A2BGR10:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0xC0000000;
				desc.pixel_format.r_bit_mask = 0x000003FF;
				desc.pixel_format.g_bit_mask = 0x000FFC00;
				desc.pixel_format.b_bit_mask = 0x3FF00000;
				break;

			case EF_SIGNED_A2BGR10:
				desc.pixel_format.flags |= DDSPF_BUMPDUDV;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0xC0000000;
				desc.pixel_format.r_bit_mask = 0x000003FF;
				desc.pixel_format.g_bit_mask = 0x000FFC00;
				desc.pixel_format.b_bit_mask = 0x3FF00000;
				break;

			case EF_GR16:
				desc.pixel_format.flags |= DDSPF_RGB;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x0000FFFF;
				desc.pixel_format.g_bit_mask = 0xFFFF0000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_SIGNED_GR16:
				desc.pixel_format.flags |= DDSPF_BUMPDUDV;
				desc.pixel_format.rgb_bit_count = 32;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x0000FFFF;
				desc.pixel_format.g_bit_mask = 0xFFFF0000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_AL4:
				desc.pixel_format.flags |= DDSPF_LUMINANCE;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 8;

				desc.pixel_format.rgb_alpha_bit_mask = 0x000000F0;
				desc.pixel_format.r_bit_mask = 0x0000000F;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_L8:
				desc.pixel_format.flags |= DDSPF_LUMINANCE;
				desc.pixel_format.rgb_bit_count = 8;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x000000FF;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_AL8:
				desc.pixel_format.flags |= DDSPF_LUMINANCE;
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x0000FF00;
				desc.pixel_format.r_bit_mask = 0x000000FF;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_L16:
				desc.pixel_format.flags |= DDSPF_LUMINANCE;
				desc.pixel_format.rgb_bit_count = 16;

				desc.pixel_format.rgb_alpha_bit_mask = 0x00000000;
				desc.pixel_format.r_bit_mask = 0x0000FFFF;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			case EF_A8:
				desc.pixel_format.flags |= DDSPF_ALPHAPIXELS;
				desc.pixel_format.rgb_bit_count = 8;

				desc.pixel_format.rgb_alpha_bit_mask = 0x000000FF;
				desc.pixel_format.r_bit_mask = 0x00000000;
				desc.pixel_format.g_bit_mask = 0x00000000;
				desc.pixel_format.b_bit_mask = 0x00000000;
				break;

			default:
				BOOST_ASSERT(false);
				break;
			}
		}

		desc.dds_caps.caps1 = DDSCAPS_TEXTURE;
		if (texture_sys_mem->NumMipMaps() != 1)
		{
			desc.dds_caps.caps1 |= DDSCAPS_MIPMAP;
			desc.dds_caps.caps1 |= DDSCAPS_COMPLEX;
		}
		if (Texture::TT_3D == texture_sys_mem->Type())
		{
			desc.dds_caps.caps1 |= DDSCAPS_COMPLEX;
			desc.dds_caps.caps2 |= DDSCAPS2_VOLUME;
			desc.flags |= DDSD_DEPTH;
			desc.depth = texture_sys_mem->Depth(0);
		}
		if (Texture::TT_Cube == texture_sys_mem->Type())
		{
			desc.dds_caps.caps1 |= DDSCAPS_COMPLEX;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_POSITIVEX;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_NEGATIVEX;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_POSITIVEY;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_NEGATIVEY;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_POSITIVEZ;
			desc.dds_caps.caps2 |= DDSCAPS2_CUBEMAP_NEGATIVEZ;
		}

		uint32_t format_size = NumFormatBytes(texture_sys_mem->Format());
		uint32_t main_image_size = texture_sys_mem->Width(0) * texture_sys_mem->Height(0) * format_size;
		if (IsCompressedFormat(texture_sys_mem->Format()))
		{
			if (EF_BC1 == texture_sys_mem->Format())
			{
				main_image_size = texture_sys_mem->Width(0) * texture_sys_mem->Height(0) / 2;
			}
			else
			{
				main_image_size = texture_sys_mem->Width(0) * texture_sys_mem->Height(0);
			}

			desc.flags |= DDSD_LINEARSIZE;
			desc.linear_size = main_image_size;
		}

		file.write(reinterpret_cast<char*>(&desc), sizeof(desc));

		switch (texture_sys_mem->Type())
		{
		case Texture::TT_1D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t image_size;
					if (IsCompressedFormat(texture_sys_mem->Format()))
					{
						int block_size;
						if (EF_BC1 == texture_sys_mem->Format())
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						image_size = ((texture_sys_mem->Width(level) + 3) / 4) * block_size;
					}
					else
					{
						image_size = main_image_size / (1UL << (level * 2));
					}

					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Read_Only, 0, texture_sys_mem->Width(level));
						file.write(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
					}
				}
			}
			break;

		case Texture::TT_2D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t width = texture_sys_mem->Width(level);
					uint32_t height = texture_sys_mem->Height(level);

					if (IsCompressedFormat(texture_sys_mem->Format()))
					{
						int block_size;
						if (EF_BC1 == texture_sys_mem->Format())
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						uint32_t image_size = ((width + 3) / 4) * ((height + 3) / 4) * block_size;

						{
							Texture::Mapper mapper(*texture_sys_mem, level, TMA_Read_Only, 0, 0, width, height);
							file.write(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
						}
					}
					else
					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Read_Only, 0, 0, width, height);
						char* data = mapper.Pointer<char>();

						for (uint32_t y = 0; y < height; ++ y)
						{
							file.write(data, static_cast<std::streamsize>(width * format_size));
							data += mapper.RowPitch();
						}
					}
				}
			}
			break;

		case Texture::TT_3D:
			{
				for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
				{
					uint32_t width = texture_sys_mem->Width(level);
					uint32_t height = texture_sys_mem->Height(level);
					uint32_t depth = texture_sys_mem->Depth(level);

					if (IsCompressedFormat(texture_sys_mem->Format()))
					{
						int block_size;
						if (EF_BC1 == texture_sys_mem->Format())
						{
							block_size = 8;
						}
						else
						{
							block_size = 16;
						}

						uint32_t image_size = ((width + 3) / 4) * ((height + 3) / 4) * depth * block_size;

						{
							Texture::Mapper mapper(*texture_sys_mem, level, TMA_Read_Only, 0, 0, 0, width, height, depth);
							file.write(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
						}
					}
					else
					{
						Texture::Mapper mapper(*texture_sys_mem, level, TMA_Read_Only, 0, 0, 0, width, height, depth);
						char* data = mapper.Pointer<char>();

						for (uint32_t z = 0; z < depth; ++ z)
						{
							for (uint32_t y = 0; y < height; ++ y)
							{
								file.write(data, static_cast<std::streamsize>(width * format_size));
								data += mapper.RowPitch();
							}

							data += mapper.SlicePitch() - mapper.RowPitch() * height;
						}
					}
				}
			}
			break;

		case Texture::TT_Cube:
			{
				for (uint32_t face = Texture::CF_Positive_X; face <= Texture::CF_Negative_Z; ++ face)
				{
					for (uint32_t level = 0; level < desc.mip_map_count; ++ level)
					{
						uint32_t width = texture_sys_mem->Width(level);
						uint32_t height = texture_sys_mem->Height(level);

						if (IsCompressedFormat(texture_sys_mem->Format()))
						{
							int block_size;
							if (EF_BC1 == texture_sys_mem->Format())
							{
								block_size = 8;
							}
							else
							{
								block_size = 16;
							}

							uint32_t image_size = ((width + 3) / 4) * ((height + 3) / 4) * block_size;

							{
								Texture::Mapper mapper(*texture_sys_mem, static_cast<Texture::CubeFaces>(face), level, TMA_Read_Only, 0, 0, width, height);
								file.write(mapper.Pointer<char>(), static_cast<std::streamsize>(image_size));
							}
						}
						else
						{
							Texture::Mapper mapper(*texture_sys_mem, static_cast<Texture::CubeFaces>(face), level, TMA_Read_Only, 0, 0, width, height);
							char* data = mapper.Pointer<char>();

							for (uint32_t y = 0; y < height; ++ y)
							{
								file.write(data, static_cast<std::streamsize>(width * format_size));
								data += mapper.RowPitch();
							}
						}
					}
				}
			}
			break;
		}
	}


	class NullTexture : public Texture
	{
	public:
		NullTexture(TextureType type, uint32_t access_hint)
			: Texture(type, access_hint)
		{
		}

		std::wstring const & Name() const
		{
			static std::wstring const name(L"Null Texture");
			return name;
		}

        uint32_t Width(int /*level*/) const
		{
			return 0;
		}
		uint32_t Height(int /*level*/) const
		{
			return 0;
		}
		uint32_t Depth(int /*level*/) const
		{
			return 0;
		}

		void CopyToTexture(Texture& /*target*/)
		{
		}

		void CopyToTexture1D(Texture& /*target*/, int /*level*/,
			uint32_t /*dst_width*/, uint32_t /*dst_xOffset*/, uint32_t /*src_width*/, uint32_t /*src_xOffset*/)
		{
		}

		void CopyToTexture2D(Texture& /*target*/, int /*level*/,
				uint32_t /*dst_width*/, uint32_t /*dst_height*/, uint32_t /*dst_xOffset*/, uint32_t /*dst_yOffset*/,
				uint32_t /*src_width*/, uint32_t /*src_height*/, uint32_t /*src_xOffset*/, uint32_t /*src_yOffset*/)
		{
		}

		void CopyToTexture3D(Texture& /*target*/, int /*level*/,
				uint32_t /*dst_width*/, uint32_t /*dst_height*/, uint32_t /*dst_depth*/,
				uint32_t /*dst_xOffset*/, uint32_t /*dst_yOffset*/, uint32_t /*dst_zOffset*/,
				uint32_t /*src_width*/, uint32_t /*src_height*/, uint32_t /*src_depth*/,
				uint32_t /*src_xOffset*/, uint32_t /*src_yOffset*/, uint32_t /*src_zOffset*/)
		{
		}

		void CopyToTextureCube(Texture& /*target*/, CubeFaces /*face*/, int /*level*/,
				uint32_t /*dst_width*/, uint32_t /*dst_height*/, uint32_t /*dst_xOffset*/, uint32_t /*dst_yOffset*/,
				uint32_t /*src_width*/, uint32_t /*src_height*/, uint32_t /*src_xOffset*/, uint32_t /*src_yOffset*/)
		{
		}

		void Map1D(int /*level*/, TextureMapAccess /*level*/,
			uint32_t /*x_offset*/, uint32_t /*width*/,
			void*& /*data*/)
		{
		}
		void Map2D(int /*level*/, TextureMapAccess /*level*/,
			uint32_t /*x_offset*/, uint32_t /*y_offset*/, uint32_t /*width*/, uint32_t /*height*/,
			void*& /*data*/, uint32_t& /*row_pitch*/)
		{
		}
		void Map3D(int /*level*/, TextureMapAccess /*level*/,
			uint32_t /*x_offset*/, uint32_t /*y_offset*/, uint32_t /*z_offset*/,
			uint32_t /*width*/, uint32_t /*height*/, uint32_t /*depth*/,
			void*& /*data*/, uint32_t& /*row_pitch*/, uint32_t& /*slice_pitch*/)
		{
		}
		void MapCube(CubeFaces /*level*/, int /*level*/, TextureMapAccess /*level*/,
			uint32_t /*x_offset*/, uint32_t /*y_offset*/, uint32_t /*width*/, uint32_t /*height*/,
			void*& /*data*/, uint32_t& /*row_pitch*/)
		{
		}

		void Unmap1D(int /*level*/)
		{
		}
		void Unmap2D(int /*level*/)
		{
		}
		void Unmap3D(int /*level*/)
		{
		}
		void UnmapCube(CubeFaces /*face*/, int /*level*/)
		{
		}

		void BuildMipSubLevels()
		{
		}
	};


	Texture::Texture(Texture::TextureType type, uint32_t access_hint)
			: type_(type), access_hint_(access_hint)
	{
	}

	Texture::~Texture()
	{
	}

	TexturePtr Texture::NullObject()
	{
		static TexturePtr obj(new NullTexture(TT_2D, 0));
		return obj;
	}

	uint16_t Texture::NumMipMaps() const
	{
		return numMipMaps_;
	}

	uint32_t Texture::Bpp() const
	{
		return bpp_;
	}

	ElementFormat Texture::Format() const
	{
		return format_;
	}

	Texture::TextureType Texture::Type() const
	{
		return type_;
	}

	uint32_t Texture::AccessHint() const
	{
		return access_hint_;
	}
}
