#pragma once

#include <QWidget>
#include <elements/basic.h>
#include <elements/sprite.h>
#include <uic/ui_texture_widget.h>

#include "../extension/view.h"
#include "../../elang_qt_globals.h"

namespace el
{
	class _ELANGQT_EXPORT QElangTextureWidget : public QWidget
	{
		Q_OBJECT

	public:
		QElangTextureWidget(QWidget* parent = Q_NULLPTR, bool internalLoop = false);
		virtual ~QElangTextureWidget() { release(); }

		void loop();
		void updateMaterial(asset<EditorProjectMaterial>);
		QElangViewSignaled* view();

		virtual void release() {
			if (mMainCam) {
				ui.view->makeCurrent();
				mMainCam.destroy();
				mPainter.destroy();
			}
		}
		void safeCreateObjects();
	protected:
		Ui::QElangTextureWidgetUI ui;
		virtual void onTextureUpdate() {};

		Qt::CursorShape mMoveCursor;
		aabb mCamBounds;
		vec2 mMoveDelta;
		vec2 mMoveCenter;
		vec2 mScrollStep;
		float mResolution;

		Box mMainCamBox, mTextureBox;
		asset<Texture> mTexture;
		asset<EditorCamera> mMainCam;
		asset<EditorProjectPainter> mPainter;
		obj<EditorProjectSprite> mTexObj;

		bool mSuppressScroll;
		bool mMovingScreen;

		void connectMouseInput();
		void syncCamera(); // fixed
		void syncScrollBars(); // fixed
	};

}





