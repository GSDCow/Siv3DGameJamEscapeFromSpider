#include <Siv3D.hpp>

//ゲームのステート
enum class GameState {
	Title,
	Gameplay,
	GameOver,
	GameClear
};

// プレイヤーの移動やカメラの制御を行うクラス
class PlayerController
{
public:
	// プレイヤーの位置
	Vec3 m_eyePosition{ 100, 2, -16 };
	Vec3 m_nextPosition{ 0, 0, 0 };
	// 水平角度
	double m_angle = 0_deg;
	// 垂直角度
	double m_pitch = 0_deg;
	// 最後に取得したマウスの位置
	Point m_lastMousePos = Cursor::Pos();

	// 角度から方向ベクトルを取得
	Vec3 GetDirection(double angle, double pitch)
	{
		Vec2 xzDir = Circular{ 1.0, angle };
		double yDir = Sin(pitch);
		return{ xzDir.x, yDir, -xzDir.y };
	}

	// 角度から水平方向のベクトルを取得
	Vec3 GetHorizontalXZDirection(double angle)
	{
		Vec2 xzDir = Circular{ 1.0, angle };
		return { xzDir.x, 0.0, -xzDir.y };
	}

public:
	// PlayerControllerクラス内のUpdatePositionメソッド
	Vec3 UpdatePosition(const Box& boundingBox)
	{
		Vec3 moveDirection(0.0, 0.0, 0.0);
		const double deltaTime = Scene::DeltaTime();
		const double speed = deltaTime * 8.0;

		// WASDキーの入力に応じて移動方向を設定
		if (KeyW.pressed()) { moveDirection += GetHorizontalXZDirection(m_angle); }
		if (KeyA.pressed()) { moveDirection += GetHorizontalXZDirection(m_angle - 90_deg); }
		if (KeyS.pressed()) { moveDirection -= GetHorizontalXZDirection(m_angle); }
		if (KeyD.pressed()) { moveDirection += GetHorizontalXZDirection(m_angle + 90_deg); }

		// 次の位置を計算
		Vec3 nextPosition = m_eyePosition + moveDirection * speed;
		Sphere nextPlayerSphere(nextPosition, 0.5);

		// バウンディングボックスとの衝突判定
		if (!nextPlayerSphere.intersects(boundingBox))
		{
			// 衝突していない場合、移動を許可
			m_eyePosition = nextPosition;
		}
		return m_eyePosition;
	}

	// マウスの操作を処理
	void HandleMouse()
	{
		// 現在のマウス位置を取得
		Point currentMousePos = Cursor::Pos();
		// 前回のマウス位置との差分を計算
		Point delta = currentMousePos - m_lastMousePos;
		// 現在のフレームではマウスカーソルを非表示にする
		Cursor::RequestStyle(CursorStyle::Hidden);
		// deltaを使用してカメラの回転を制御
		m_angle += delta.x * 0.3_deg;
		m_pitch -= delta.y * 0.3_deg;
		m_pitch = Clamp(m_pitch, -80_deg, 80_deg);
		// マウスカーソルをウィンドウの中心に戻す
		Size windowSize = Scene::Size();
		Point windowCenter(windowSize.x / 2, windowSize.y / 2);
		Cursor::SetPos(windowCenter);
		m_lastMousePos = windowCenter;  // 前回のマウス位置を更新
		// 現在のフレームではマウスカーソルを非表示にする
		Cursor::RequestStyle(CursorStyle::Hidden);
	}

	// プレイヤーが注目している位置を返す
	Vec3 GetFocusPosition()
	{
		return m_eyePosition + GetDirection(m_angle, m_pitch);
	}
};

// スパイダーのバウンディングボックスを取得する関数
Box GetSpiderBoundingBox(const Model& spiderModel, const Vec3& position)
{
	// 元のスパイダーモデルのバウンディングボックスを取得
	Box box = spiderModel.boundingBox();
	// サイズを少し大きくする
	box = box.stretched(0.3);
	// 現在のスパイダーの位置に移動させる
	box = box.movedBy(position);
	return box;
}

//ライトの設定
struct PSLighting
{
	static constexpr uint32 MaxPointLights = 4;

	struct Light
	{
		Float4 position{ 0, 0, 0, 0 };
		Float4 diffuseColor{ 0, 0, 0, 0 };
		Float4 attenuation{ 1.0f, 2.0f, 1.0f, 0 };
	};

	std::array<Light, MaxPointLights> pointLights;

	/// @brief 点光源を設定します。
	/// @param i 光源のインデックス。0 以上 MaxPointLights 未満である必要があります。
	/// @param pos 光源の位置
	/// @param diffuse 色
	/// @param r 強さ
	void setPointLight(uint32 i, const Vec3& pos, const ColorF& diffuse, double r)
	{
		pointLights[i].position = Float4{ pos, 1.0f };
		pointLights[i].diffuseColor = diffuse.toFloat4();
		pointLights[i].attenuation = Float4{ 1.0, (2.0 / r), (1.0 / (r * r)), 0.0 };
	}

	/// @brief 点光源を球として描画します。
	/// @param i 光源のインデックス。0 以上 MaxPointLights 未満である必要があります。
	/// @param r 球の半径
	void drawPointLightAsEmissiveSphere(uint32 i, double r)
	{
		const Vec3 pos = pointLights[i].position.xyz();
		const ColorF diffuse{ pointLights[i].diffuseColor };

		PhongMaterial phong;
		phong.ambientColor = ColorF{ 0.0 };
		phong.diffuseColor = ColorF{ 0.0 };
		phong.emissionColor = diffuse;
		Sphere{ pos, r }.draw(phong);
	}
};

//Fogの設定
struct PSFog
{
	Float3 fogColor;
	float fogCoefficient;
};

// メイン関数
void Main()
{
	// ウィンドウのサイズを設定
	Window::Resize(1280, 720);
	Window::SetTitle(U"EscapeFromSpider");
	//-------------------------------------------------------------------------
	// 
	//タイトル系の宣言
	//最初はステートをタイトルに設定
	GameState currentState = GameState::Title;

	//ボタンを設定
	Rect startButton(Scene::Center().x - 60, Scene::Center().y + 250, 150, 50);

	// 画像をロード
	Texture background(U"Assets/background.png");
	Texture Title(U"Assets/Title.png");
	Texture SpiderWeb(U"Assets/SpiderWeb.png");
	Texture escape(U"Assets/escape.png");
	Texture eat(U"Assets/eat.png");
	Texture EggPNG(U"Assets/EggPNG.png");
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	// 
	//インゲーム系の宣言
	//BGMを読み込む
	Audio bgm(U"Assets/bgm.mp3");
	bgm.setVolume(0.1);
	bgm.setLoop(true);
	Audio fire(U"Assets/fire.mp3");
	Audio toClose(U"Assets/toClose.mp3");
	//背景色を設定
	const ColorF backgroundColor = ColorF{ 0.1, 0.1, 0.1 }.removeSRGBCurve();
	//レンダリング用のテクスチャを設定
	const MSRenderTexture renderTexture{ Scene::Size(), TextureFormat::R8G8B8A8_Unorm_SRGB, HasDepth::Yes };

	//マップの上下をロード
	const Model model(U"Assets/map.obj");
	//スパイダーをロード
	const Model Spider(U"Assets/Spider.obj");
	//ライターをロード
	const Model Lighter(U"Assets/Lighter.obj");
	//マップのモデルをロード(大量）
	const Model walldouble(U"Assets/MapModels/walldouble.obj");
	const Model walldouble1(U"Assets/MapModels/walldouble1.obj");
	const Model walldouble2(U"Assets/MapModels/walldouble2.obj");
	const Model walldouble3(U"Assets/MapModels/walldouble3.obj");
	const Model walldouble4(U"Assets/MapModels/walldouble4.obj");
	const Model walldouble5(U"Assets/MapModels/walldouble5.obj");
	const Model walldouble6(U"Assets/MapModels/walldouble6.obj");
	const Model walldouble7(U"Assets/MapModels/walldouble7.obj");
	const Model walldouble8(U"Assets/MapModels/walldouble8.obj");
	const Model walldouble9(U"Assets/MapModels/walldouble9.obj");
	const Model walldouble10(U"Assets/MapModels/walldouble10.obj");
	const Model walldouble11(U"Assets/MapModels/walldouble11.obj");
	const Model walldouble12(U"Assets/MapModels/walldouble12.obj");
	const Model walldouble13(U"Assets/MapModels/walldouble13.obj");
	const Model walldouble14(U"Assets/MapModels/walldouble14.obj");
	const Model walldouble15(U"Assets/MapModels/walldouble15.obj");
	const Model walldouble16(U"Assets/MapModels/walldouble16.obj");
	const Model walldouble17(U"Assets/MapModels/walldouble17.obj");
	const Model walldouble18(U"Assets/MapModels/walldouble18.obj");
	const Model walldouble19(U"Assets/MapModels/walldouble19.obj");
	const Model walldouble20(U"Assets/MapModels/walldouble20.obj");
	const Model walldouble21(U"Assets/MapModels/walldouble21.obj");
	const Model walldouble22(U"Assets/MapModels/walldouble22.obj");
	const Model walldouble23(U"Assets/MapModels/walldouble23.obj");
	const Model walldouble24(U"Assets/MapModels/walldouble24.obj");

	const Model wallbox(U"Assets/MapModels/wallbox.obj");
	const Model wallbox1(U"Assets/MapModels/wallbox1.obj");
	const Model wallbox2(U"Assets/MapModels/wallbox2.obj");
	const Model wallbox3(U"Assets/MapModels/wallbox3.obj");
	const Model wallbox4(U"Assets/MapModels/wallbox4.obj");
	const Model wallbox5(U"Assets/MapModels/wallbox5.obj");
	const Model wallbox6(U"Assets/MapModels/wallbox6.obj");

	const Model egg(U"Assets/MapModels/egg.obj");
	const Model egg1(U"Assets/MapModels/egg1.obj");
	const Model egg2(U"Assets/MapModels/egg2.obj");
	const Model egg3(U"Assets/MapModels/egg3.obj");
	const Model egg4(U"Assets/MapModels/egg4.obj");
	//当たり判定
	const Box walldoubleBox = walldouble.boundingBox();
	const Box walldoubleBox1 = walldouble1.boundingBox();
	const Box walldoubleBox2 = walldouble2.boundingBox();
	const Box walldoubleBox3 = walldouble3.boundingBox();
	const Box walldoubleBox4 = walldouble4.boundingBox();
	const Box walldoubleBox5 = walldouble5.boundingBox();
	const Box walldoubleBox6 = walldouble6.boundingBox();
	const Box walldoubleBox7 = walldouble7.boundingBox();
	const Box walldoubleBox8 = walldouble8.boundingBox();
	const Box walldoubleBox9 = walldouble9.boundingBox();
	const Box walldoubleBox10 = walldouble10.boundingBox();
	const Box walldoubleBox11 = walldouble11.boundingBox();
	const Box walldoubleBox12 = walldouble12.boundingBox();
	const Box walldoubleBox13 = walldouble13.boundingBox();
	const Box walldoubleBox14 = walldouble14.boundingBox();
	const Box walldoubleBox15 = walldouble15.boundingBox();
	const Box walldoubleBox16 = walldouble16.boundingBox();
	const Box walldoubleBox17 = walldouble17.boundingBox();
	const Box walldoubleBox18 = walldouble18.boundingBox();
	const Box walldoubleBox19 = walldouble19.boundingBox();
	const Box walldoubleBox20 = walldouble20.boundingBox();
	const Box walldoubleBox21 = walldouble21.boundingBox();
	const Box walldoubleBox22 = walldouble22.boundingBox();
	const Box walldoubleBox23 = walldouble23.boundingBox();
	const Box walldoubleBox24 = walldouble24.boundingBox();

	const Box wallboxBox = wallbox.boundingBox();
	const Box wallboxBox1 = wallbox1.boundingBox();
	const Box wallboxBox2 = wallbox2.boundingBox();
	const Box wallboxBox3 = wallbox3.boundingBox();
	const Box wallboxBox4 = wallbox4.boundingBox();
	const Box wallboxBox5 = wallbox5.boundingBox();
	const Box wallboxBox6 = wallbox6.boundingBox();

	const Box eggBox = egg.boundingBox();
	const Box eggBox1 = egg1.boundingBox();
	const Box eggBox2 = egg2.boundingBox();
	const Box eggBox3 = egg3.boundingBox();
	const Box eggBox4 = egg4.boundingBox();

	bool eggFire = false;
	bool eggFire1 = false;
	bool eggFire2 = false;
	bool eggFire3 = false;
	bool eggFire4 = false;

	// カスタムピクセルシェーダ
	const PixelShader ps3D = HLSL{ U"Assets/point_light.hlsl", U"PS" };
	ConstantBuffer<PSLighting> constantBuffer;
	if (not ps3D)
	{
		return;
	}
	const PixelShader ps = HLSL{ U"example/shader/hlsl/forward_fog.hlsl", U"PS" }
	| GLSL{ U"example/shader/glsl/forward_fog.frag", { { U"PSPerFrame", 0 }, { U"PSPerView", 1 }, { U"PSPerMaterial", 3 }, { U"PSFog", 4 } } };
	if (not ps)
	{
		return;
	}
	double fogParam = 0.6;
	ConstantBuffer<PSFog> cb{ { backgroundColor.rgb(), 0.0f } };

	// 3Dカメラの設定
	BasicCamera3D camera{ renderTexture.size(), 60_deg, Vec3{ 0, 16, -32 }, Vec3{ 0, 0, 0 } };

	// 3Dのライティング設定
	Graphics3D::SetGlobalAmbientColor(ColorF(0.1, 0.1, 0.1));
	Graphics3D::SetSunDirection(Vec3{ 1, -1, -1 }.normalized());
	Graphics3D::SetSunColor(ColorF(0.1));

	// モデルのバウンディングボックスを取得と移動
	const Box originalBoundingBox = model.boundingBox();
	const Box scaledBoundingBox = originalBoundingBox.scaled(0.1);
	const Box boundingBox = scaledBoundingBox.movedBy(0.0, -100.0, 0.0);

	//スパイダーモデルの当たり判定
	const Box spiderBoundingBox = Spider.boundingBox();
	// スパイダーモデルの初期位置
	Vec3 spiderPosition{ 0, -0.62, 10 };

	// PlayerControllerのインスタンス作成
	PlayerController playerController;

	// 前フレームのプレイヤーの位置を保持する変数
	Vec3 previousPlayerPosition = playerController.m_eyePosition;

	double alpha = 0.2;
	bool increasing = true;
	// アップデート
	while (System::Update())
	{
		switch (currentState)
		{

		//タイトル
		case GameState::Title:
		{
			//画像を背景として描画
			background.draw();
			const Vec2 center = Scene::Center();
			const double rotation = Scene::Time() * 0.1; // 回転角度を設定
			// SpiderWeb を回転させて描画
			SpiderWeb.rotated(rotation).drawAt(center, ColorF(1.0, 1.0, 1.0, alpha));
			// アルファ値を変更
			if (increasing) {
				alpha += 0.0001; // アルファ値を増加させる
				if (alpha >= 0.1) {
					increasing = false; // アルファ値が0.2に達したら減少方向に切り替え
				}
			}
			else {
				alpha -= 0.0001; // アルファ値を減少させる
				if (alpha <= 0.0) {
					increasing = true; // アルファ値が0に達したら増加方向に切り替え
				}
			}
			EggPNG.draw();
			Title.draw();
			//ボタンが押されたらゲームプレイに遷移
			if (SimpleGUI::Button(U"StartGame", startButton.leftCenter(), 100))
			{
				currentState = GameState::Gameplay;
			}
		}
		break;

		//ゲームオーバー
		case GameState::GameOver:
		{
			//画像を背景として描画
			background.draw();
			eat.draw();
			//ボタンが押されたらゲームプレイに遷移
			if (SimpleGUI::Button(U"RestartGame", startButton.leftCenter(), 100))
			{
				currentState = GameState::Gameplay;
			}
		}
		break;

		//ゲームクリア
		case GameState::GameClear:
		{
			//画像を背景として描画
			background.draw();
			escape.draw();
			//ボタンが押されたらゲームプレイに遷移
			if (SimpleGUI::Button(U"BacktoTitle", startButton.leftCenter(), 100))
			{
				//リセット
				eggFire = false;
				eggFire1 = false;
				eggFire2 = false;
				eggFire3 = false;
				eggFire4 = false;
				spiderPosition = { 0, -0.62, 10 };
				playerController.m_eyePosition = { 100, 2, -16 };
				currentState = GameState::Title;
			}
		}
		break;

		//インゲーム
		case GameState::Gameplay:
		{
			//ゲームクリア処理
			if (eggFire && eggFire1 && eggFire2 && eggFire3 && eggFire4)
			{
				Print << U"卵を全て燃やした！ゲームクリア！";
				currentState = GameState::GameClear;
			}
			bgm.play();//bgmを再生
			//fogの処理
			const double fogCoefficient = Math::Eerp(0.001, 0.5, fogParam);
			cb->fogCoefficient = static_cast<float>(fogCoefficient);
			const ScopedCustomShader3D shader{ ps3D };
			//時間
			const double deltaTime = Scene::DeltaTime();
			// マウスの処理
			playerController.HandleMouse();
			// プレイヤーの位置を更新
			const Vec3 eyePosition = playerController.UpdatePosition(boundingBox);
			// カメラのビューを更新
			camera.setView(eyePosition, playerController.GetFocusPosition());
			Graphics3D::SetCameraTransform(camera);

			// 3Dレンダリング
			{
				const ScopedRenderTarget3D target{ renderTexture.clear(backgroundColor) };
				//Fog
				Graphics3D::SetPSConstantBuffer(-0.3, cb);
				model.draw(Mat4x4::Scale(1));
				boundingBox.drawFrame(Palette::Red);

				// プレイヤーの現在位置を球で表示
				const Sphere playerSphere(eyePosition, 0.5);
				playerSphere.draw(Palette::Blue);
				// プレイヤーに向かう方向ベクトルを計算
				Vec3 directionToPlayer = (eyePosition - spiderPosition);
				directionToPlayer.y = 0; // y成分を0にする
				directionToPlayer = directionToPlayer.normalized();
				// Y軸を中心とした回転角度を計算
				double yaw = Atan2(directionToPlayer.x, directionToPlayer.z);
				// Spiderの変換行列を生成
				Mat4x4 spiderTransform = Mat4x4::Scale(1) * Mat4x4::RotateY(yaw) * Mat4x4::Translate(spiderPosition);
				// 描画
				Spider.draw(spiderTransform);
				// Spiderがプレイヤーに向かって移動する速度
				const double spiderSpeed = deltaTime * 4.0;  // 2.0はSpiderの移動速度
				// Spiderの位置を更新
				spiderPosition += directionToPlayer * spiderSpeed;

				// スパイダーのバウンディングボックスを動的に更新
				Box dynamicSpiderBoundingBox = GetSpiderBoundingBox(Spider, spiderPosition);
				//dynamicSpiderBoundingBox.drawFrame(Palette::Green);

				//マップ表示
				walldouble.draw();
				walldouble1.draw();
				walldouble2.draw();
				walldouble3.draw();
				walldouble4.draw();
				walldouble5.draw();
				walldouble6.draw();
				walldouble7.draw();
				walldouble8.draw();
				walldouble9.draw();
				walldouble10.draw();
				walldouble11.draw();
				walldouble12.draw();
				walldouble13.draw();
				walldouble14.draw();
				walldouble15.draw();
				walldouble16.draw();
				walldouble17.draw();
				walldouble18.draw();
				walldouble19.draw();
				walldouble20.draw();
				walldouble21.draw();
				walldouble22.draw();
				walldouble23.draw();
				walldouble24.draw();

				wallbox.draw();
				wallbox1.draw();
				wallbox2.draw();
				wallbox3.draw();
				wallbox4.draw();
				wallbox5.draw();
				wallbox6.draw();

				egg.draw();
				egg1.draw();
				egg2.draw();
				egg3.draw();
				egg4.draw();

				//walldoubleBox.drawFrame(Palette::Red);
				//walldoubleBox1.drawFrame(Palette::Red);
				//walldoubleBox2.drawFrame(Palette::Red);
				//walldoubleBox3.drawFrame(Palette::Red);
				//walldoubleBox4.drawFrame(Palette::Red);
				//walldoubleBox5.drawFrame(Palette::Red);
				//walldoubleBox6.drawFrame(Palette::Red);
				//walldoubleBox7.drawFrame(Palette::Red);
				//walldoubleBox8.drawFrame(Palette::Red);
				//walldoubleBox9.drawFrame(Palette::Red);
				//walldoubleBox10.drawFrame(Palette::Red);
				//walldoubleBox11.drawFrame(Palette::Red);
				//walldoubleBox12.drawFrame(Palette::Red);
				//walldoubleBox13.drawFrame(Palette::Red);
				//walldoubleBox14.drawFrame(Palette::Red);
				//walldoubleBox15.drawFrame(Palette::Red);
				//walldoubleBox16.drawFrame(Palette::Red);
				//walldoubleBox17.drawFrame(Palette::Red);
				//walldoubleBox18.drawFrame(Palette::Red);
				//walldoubleBox19.drawFrame(Palette::Red);
				//walldoubleBox20.drawFrame(Palette::Red);
				//walldoubleBox21.drawFrame(Palette::Red);
				//walldoubleBox22.drawFrame(Palette::Red);
				//walldoubleBox23.drawFrame(Palette::Red);
				//walldoubleBox24.drawFrame(Palette::Red);

				//wallboxBox.drawFrame(Palette::Green);
				//wallboxBox1.drawFrame(Palette::Green);
				//wallboxBox2.drawFrame(Palette::Green);
				//wallboxBox3.drawFrame(Palette::Green);
				//wallboxBox4.drawFrame(Palette::Green);
				//wallboxBox5.drawFrame(Palette::Green);
				//wallboxBox6.drawFrame(Palette::Green);

				//eggBox.drawFrame(Palette::Orange);
				//eggBox1.drawFrame(Palette::Orange);
				//eggBox2.drawFrame(Palette::Orange);
				//eggBox3.drawFrame(Palette::Orange);
				//eggBox4.drawFrame(Palette::Orange);

				// ライターモデルをカメラの前に表示
				Vec3 cameraDirection = playerController.GetFocusPosition() - eyePosition;
				Vec3 offsetFromCamera = cameraDirection.cross(Vec3{ 0, 1, 0 }).normalized() * -0.1; // 右方向へのオフセット
				offsetFromCamera.y -= 0.1; // 下方向へのオフセット
				Vec3 lighterPosition = eyePosition + cameraDirection.normalized() * 0.3 + offsetFromCamera;
				Vec3 lightPosition = lighterPosition;
				// カスタムシェーダを使用する
				const ScopedCustomShader3D shader{ ps3D };
				// ピクセルシェーダに定数バッファを渡す
				Graphics3D::SetPSConstantBuffer(4, constantBuffer);
				constantBuffer->setPointLight(0, lightPosition, ColorF{ 1.0, 0.2, 0.0 }, 5.0);
				// その変換行列を使用してモデルを描画
				Lighter.draw(lighterPosition);

				//卵を燃やしたときの処理
				if (playerSphere.intersects(eggBox)) {if (MouseL.down()) {Print << U"eggFire!"; eggFire = true; fire.play();}}
				if (playerSphere.intersects(eggBox1)) { if (MouseL.down()) { Print << U"eggFire1!"; eggFire1 = true; fire.play();}}
				if (playerSphere.intersects(eggBox2)) { if (MouseL.down()) { Print << U"eggFire2!"; eggFire2 = true; fire.play();}}
				if (playerSphere.intersects(eggBox3)) { if (MouseL.down()) { Print << U"eggFire3!"; eggFire3 = true; fire.play();}}
				if (playerSphere.intersects(eggBox4)) { if (MouseL.down()) { Print << U"eggFire4!"; eggFire4 = true; fire.play();}}
				//蜘蛛に接触したとき
				if (playerSphere.intersects(dynamicSpiderBoundingBox))
				{
					//Print << U"餌になった..";
					//リセット
					eggFire = false;
					eggFire1 = false;
					eggFire2 = false;
					eggFire3 = false;
					eggFire4 = false;
					spiderPosition = { 0, -0.62, 10 };
					playerController.m_eyePosition = { 100, 2, -16 };
					currentState = GameState::GameOver;
				}

				// プレイヤーの位置を更新
				Vec3 eyePosition = playerController.UpdatePosition(boundingBox);

				// プレイヤーが壁に当たった場合の処理
				if (playerSphere.intersects(walldoubleBox)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox1)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox2)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox3)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox4)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox5)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox6)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox7)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox8)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox9)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox10)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox11)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox12)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox13)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox14)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox15)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox16)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox17)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox18)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox19)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox20)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox21)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox22)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox23)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(walldoubleBox24)) { playerController.m_eyePosition = previousPlayerPosition; }

				if (playerSphere.intersects(wallboxBox)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox1)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox2)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox3)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox4)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox5)) { playerController.m_eyePosition = previousPlayerPosition; }
				if (playerSphere.intersects(wallboxBox6)) { playerController.m_eyePosition = previousPlayerPosition; }

				// 更新後のプレイヤー位置を前フレームの位置として保持
				previousPlayerPosition = playerController.m_eyePosition;
			}

			// レンダリング結果を画面に表示
			{
				Graphics3D::Flush();
				renderTexture.resolve();
				Shader::LinearToScreen(renderTexture);
			}
		}
		break;
		}
	}
}
