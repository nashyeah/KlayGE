#include <KlayGE/KlayGE.hpp>
#include <KlayGE/Util.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/Math.hpp>
#include <KlayGE/Font.hpp>
#include <KlayGE/Renderable.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/FrameBuffer.hpp>
#include <KlayGE/SceneManager.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KlayGE/RenderSettings.hpp>
#include <KlayGE/KMesh.hpp>

#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/InputFactory.hpp>

#include <sstream>
#include <ctime>
#include <boost/bind.hpp>

#include "SkinnedMesh.hpp"

using namespace KlayGE;
using namespace std;

namespace
{
	enum
	{
		Exit,
	};

	InputActionDefine actions[] =
	{
		InputActionDefine(Exit, KS_Escape),
	};

	bool ConfirmDevice()
	{
		RenderEngine& re = Context::Instance().RenderFactoryInstance().RenderEngineInstance();
		RenderDeviceCaps const & caps = re.DeviceCaps();
		if (caps.max_shader_model < 2)
		{
			return false;
		}
		return true;
	}

	struct CreateMD5ModelFactory
	{
		RenderModelPtr operator()(std::wstring const & /*name*/)
		{
			return RenderModelPtr(new MD5SkinnedModel());
		}
	};
}

int main()
{
	ResLoader::Instance().AddPath("../../media/Common");
	ResLoader::Instance().AddPath("../../media/SkinnedMesh");

	RenderSettings settings;
	SceneManagerPtr sm = Context::Instance().LoadCfg(settings, "KlayGE.cfg");
	settings.ConfirmDevice = ConfirmDevice;

	SkinnedMeshApp app("SkinnedMesh", settings);
	app.Create();
	app.Run();

	return 0;
}

SkinnedMeshApp::SkinnedMeshApp(std::string const & name, RenderSettings const & settings)
					: App3DFramework(name, settings)
{
}

void SkinnedMeshApp::InitObjects()
{
	font_ = Context::Instance().RenderFactoryInstance().MakeFont("gkai00mp.kfont", 16);

	this->LookAt(float3(250.0f, 0.0f, 48.0f), float3(0.0f, 0.0f, 48.0f), float3(0.0f, 0.0f, 1.0f));
	this->Proj(10, 500);

	fpsController_.AttachCamera(this->ActiveCamera());
	fpsController_.Scalers(0.1f, 10);

	model_ = checked_pointer_cast<MD5SkinnedModel>(LoadKModel("pinky.kmodel", EAH_GPU_Read, CreateMD5ModelFactory(), CreateKMeshFactory<MD5SkinnedMesh>()));
	model_->SetTime(0);

	InputEngine& inputEngine(Context::Instance().InputFactoryInstance().InputEngineInstance());
	InputActionMap actionMap;
	actionMap.AddActions(actions, actions + sizeof(actions) / sizeof(actions[0]));

	action_handler_t input_handler(new input_signal);
	input_handler->connect(boost::bind(&SkinnedMeshApp::InputHandler, this, _1, _2));
	inputEngine.ActionMap(actionMap, input_handler, true);
}

void SkinnedMeshApp::InputHandler(InputEngine const & /*sender*/, InputAction const & action)
{
	switch (action.first)
	{
	case Exit:
		this->Quit();
		break;
	}
}

uint32_t SkinnedMeshApp::DoUpdate(KlayGE::uint32_t /*pass*/)
{
	RenderEngine& renderEngine(Context::Instance().RenderFactoryInstance().RenderEngineInstance());

	renderEngine.CurFrameBuffer()->Clear(FrameBuffer::CBM_Color | FrameBuffer::CBM_Depth, Color(0.2f, 0.4f, 0.6f, 1), 1.0f, 0);

	model_->SetTime(std::clock() / 1000.0f);

	std::wostringstream stream;
	stream << this->FPS();

	model_->SetEyePos(this->ActiveCamera().EyePos());

	model_->AddToRenderQueue();

	font_->RenderText(0, 0, Color(1, 1, 0, 1), renderEngine.Name());

	FrameBuffer& rw(*checked_pointer_cast<FrameBuffer>(renderEngine.CurFrameBuffer()));
	font_->RenderText(0, 18, Color(1, 1, 0, 1), rw.Description());

	font_->RenderText(0, 36, Color(1, 1, 0, 1), stream.str());

	return App3DFramework::URV_Need_Flush | App3DFramework::URV_Finished;
}
