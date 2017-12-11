// DShow.hpp
// KlayGE DirectShow ���������� ͷ�ļ�
// Ver 2.0.0
// ��Ȩ����(C) ������, 2003
// Homepage: http://www.klayge.org
//
// 2.0.0
// ���ν��� (2003.9.3)
//
// �޸ļ�¼
/////////////////////////////////////////////////////////////////////

#ifndef _DSHOW_HPP
#define _DSHOW_HPP

#pragma once

#include <windows.h>
#include <control.h>
#if defined(KLAYGE_COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment" // Ignore "/*" within block comment
#pragma GCC diagnostic ignored "-Wunknown-pragmas" // Ignore unknown pragmas
#elif defined(KLAYGE_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment" // Ignore "/*" within block comment
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value" // Ignore D3DBUSIMPL_MODIFIER_NON_STANDARD definition
#endif
#include <d3d9.h>
#if defined(KLAYGE_COMPILER_GCC)
#pragma GCC diagnostic pop
#elif defined(KLAYGE_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#ifdef KLAYGE_COMPILER_GCC
#define _WIN32_WINNT_BACKUP _WIN32_WINNT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <strmif.h>
#ifdef KLAYGE_COMPILER_GCC
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_BACKUP
#endif
#include <vmr9.h>

#include <string>

#include <KlayGE/Show.hpp>

namespace KlayGE
{
	class DShowEngine : public ShowEngine
	{
	public:
		DShowEngine();
		~DShowEngine();

		bool IsComplete();

		void Load(std::string const & fileName);
		TexturePtr PresentTexture();

		ShowState State(long msTimeout = -1);

	private:
		std::shared_ptr<IGraphBuilder>		graph_;
		std::shared_ptr<IBaseFilter>		filter_;
		std::shared_ptr<IMediaControl>		media_control_;
		std::shared_ptr<IMediaEvent>		media_event_;
		std::shared_ptr<IVMRSurfaceAllocator9> vmr_allocator_;

	private:
		void Init();
		void Free();

		virtual void DoSuspend() override;
		virtual void DoResume() override;

		void DoPlay();
		void DoStop();
		void DoPause();
	};
}

#endif		// _DSHOW_HPP
