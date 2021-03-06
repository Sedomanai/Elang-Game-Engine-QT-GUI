#include "editor.h"
#include <qopenglcontext>


namespace el
{
	AtlasEditor::AtlasEditor(QWidget* parent)
		: QMainWindow(parent), mSuppressSignal(false), mInitialized(false), mCellsWidget(0)
	{
		ui.setupUi(this);
		setupActions();

		if (gGUI.open()) {
			ui.actionNewProject->setVisible(false);
			ui.actionLoadProject->setVisible(false);
			ui.actionSaveProject->setVisible(false);
			ui.actionSaveProjectAs->setVisible(false);
			init();
			// change mode
		} else { 
			QTimer* timer = new QTimer(this);
			connect(timer, &QTimer::timeout, this, &AtlasEditor::loop);
			timer->start(1000.0f / 60.0f);
		}
	}

	void AtlasEditor::loop() {
		if (mInitialized) {
			bind(&mStage);
			mCellsWidget->loop();
			//mClipsWidget->loop();
		}
	}

	void AtlasEditor::init(bool debug) {
		if (!mInitialized) {
			mInitialized = true;
			bind(&mStage);
			// DO NOT CHANGE THE ORDER
			setupLayout(); // do not touch
			setupList(); // do not touch
			setupCellMode();
			setupOriginMode();
			setupClipMode();
			setupInitView();

			mViewActions->setEnabled(true);
			gAtlsUtil.currentMaterial = gProject->makeSub<EditorProjectMaterial>(); // delete this material later
		} else {
			mCellsWidget->release();
			mCellsWidget->safeCreateObjects();
			mOriginView->release();
			mOriginView->safeCreateObjects();
			//mClipsWidget->release();
			//mClipsWidget->safeCreateObjects();
		}

		mTextureBox->clear();
		for (auto it : gProject->textures) {
			mTextureBox->addItem(QString::fromStdString(it.first));
		}
		//mAtlasBox->clear();
		//for (auto it : gProject->atlases) {
		//	mAtlasBox->addItem(QString::fromStdString(it.first));
		//}

		if (debug) {
			mTextureBox->setCurrentIndex(1);

			//assert(gAtlsUtil.currentMaterial);
			//assert(gAtlsUtil.currentMaterial->hasTexture());
			//auto texture = gAtlsUtil.currentMaterial->textures[0];
			//if (texture && texture->atlas) {
			//	assert(gProject->atlases.contains(texture->atlas));
			//	auto text = gProject->atlases[texture->atlas];
			//	mAtlasBox->setCurrentText(QString::fromStdString(text));
			//}

			ui.actionOriginMode->trigger();
			//ui.actionClipMode->trigger();
		}
	}

	void AtlasEditor::setupLayout() {
		QHBoxLayout* layout = new QHBoxLayout();
		layout->setContentsMargins(QMargins(0, 0, 0, 0));
		mListLayout = new QVBoxLayout();
		mListLayout->setSizeConstraint(QLayout::SizeConstraint::SetFixedSize);
		layout->addLayout(mListLayout);
		mViewLayout = new QVBoxLayout();
		layout->addLayout(mViewLayout);
		ui.frame->setLayout(layout);
	}

	void AtlasEditor::setupList() {
		gAtlsUtil.cellList = new QListExtension(this);
		auto& cells = *gAtlsUtil.cellList;
		cells.setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
		cells.setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);
		cells.setDefaultDropAction(Qt::DropAction::MoveAction);
		cells.setEditTriggers(QAbstractItemView::EditTrigger::EditKeyPressed);
		mListLayout->addWidget(&cells);

		gAtlsUtil.clipList = new QListExtension(this);
		auto& clips = *gAtlsUtil.clipList;
		clips.setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
		clips.setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);
		clips.setDefaultDropAction(Qt::DropAction::MoveAction);
		clips.setEditTriggers(QAbstractItemView::EditTrigger::EditKeyPressed);
		mListLayout->addWidget(&clips);
	}

	void AtlasEditor::setupInitView() {
		mAutoGen = new AtlasAutoGenerator(this);
		mGhostDialog = new ElangAtlasGhostDialog(this);
		mViewMode = ViewMode::CELL;
		mCellsWidget->showEditor();
		gAtlsUtil.clipList->hide();
		mOriginView->hide();
		mOriginToolbar->hide();
		mClipsWidget->hide();
		mClipsToolbar->hide();
	}

	void AtlasEditor::setupCellMode() {
		mCellToolbar = new QToolBar(this);
		QLabel* label = new QLabel("  Set Texture  ", mCellToolbar);
		mTextureBox = new QComboBox(mCellToolbar);
		mTextureBox->setMinimumWidth(100);
		connect(mTextureBox, &QComboBox::currentTextChanged, [&](const QString& text) {
			if (gProject->textures.contains(text.toStdString())) {
				assert(gAtlsUtil.currentMaterial);
				gAtlsUtil.currentMaterial->setTexture(gProject->textures[text.toStdString()]);
				mCellsWidget->updateMaterial(gAtlsUtil.currentMaterial);
				mOriginView->updateTexture();
				mClipsWidget->updateTexture();
			}
		});

		QLabel* label2 = new QLabel("  Set Atlas  ", mCellToolbar);
		mAtlasBox = new QComboBox(mCellToolbar);
		mAtlasBox->setMinimumWidth(100);
		connect(mAtlasBox, &QComboBox::currentTextChanged, [&](const QString& text) {
			if (gProject->atlases.contains(text.toStdString())) {
				assert(gAtlsUtil.currentMaterial);
				if (gAtlsUtil.currentMaterial->hasTexture()) {
					auto texture = gAtlsUtil.currentMaterial->textures[0];
					asset<Atlas> atlas = gProject->atlases[text.toStdString()];

					if (texture->atlas != atlas) {
						// unlink previous
						if (texture->atlas) {
							sizet index = 0;
							auto texs = texture->atlas->textures;
							for (auto tex : texs) {
								if (tex == texture) {
									texs.erase(texs.begin() + index);
									break;
								} index++;
							}
						}

						texture->atlas = atlas;
						atlas->textures.emplace_back(texture);
						mCellsWidget->updateMaterial(gAtlsUtil.currentMaterial);
					}
				}

				mOriginView->updateTexture();
				mClipsWidget->updateTexture();
			}
		});

		mCellToolbar->addWidget(label);
		mCellToolbar->addWidget(mTextureBox);
		mCellToolbar->addSeparator();

		mCellToolbar->addWidget(label2);
		mCellToolbar->addWidget(mAtlasBox);
		mCellToolbar->addSeparator();


		//auto autogen = mCellToolbar->addAction("Auto Atlas", [&]() {


		//	});
		auto autogen = mCellToolbar->addAction("Auto Atlas", [&]() {
			mAutoGen->exec();
			if (mAutoGen->gen) {
				mCellsWidget->autoGenCells(mAutoGen->sortorder, mAutoGen->margin);
				//TODO: add warning, will remove all clips and cells
				mClipsWidget->clearAllViews();

				mAutoGen->gen = false;
			}
		}); autogen->setShortcut(QKeySequence(Qt::Key_F));

		//auto sort = mCellToolbar->addAction("Sort List", [&]() {
		//	mAutoGen->exec();
		//	if (mAutoGen->gen) {
		//		mCellsWidget->autoGenCells(mAutoGen->sortorder, mAutoGen->margin);
		//		mAutoGen->gen = false;
		//	}
		//}); autogen->setShortcut(QKeySequence(Qt::Key_R));

		auto autocell = mCellToolbar->addAction("Auto Cell", [&]() {
			mCellsWidget->autoCreateCell();
		}); autocell->setShortcut(QKeySequence(Qt::Key_R));

		mCellToolbar->addSeparator();

		auto combine = mCellToolbar->addAction("Combine Cells", [&]() {
			mCellsWidget->combineCells();
			});
		combine->setShortcut(QKeySequence(Qt::Key_C));

		auto remove = mCellToolbar->addAction("Remove Cells", [&]() {
			mCellsWidget->deleteSelected();
			});
		remove->setShortcut(QKeySequence(Qt::Key_Delete));

		mCellToolbar->addSeparator();
		
		mCellsWidget = new CellsWidget(this);
		mCellsWidget->setMinimumWidth(750);
		mCellsWidget->view()->setStage(&mStage);
		mViewLayout->addWidget(mCellsWidget);

		addToolBarBreak();
		addToolBar(Qt::ToolBarArea::TopToolBarArea, mCellToolbar);
	}

	void AtlasEditor::setupOriginMode() {
		mOriginToolbar = new QToolBar(this);
		auto act = mOriginToolbar->addAction("Set Ghost", [&]() {
			mGhostDialog->exec();
			if (mGhostDialog->confirmed()) {
				//cout << "confirmed" << endl;
				mOriginView->setGhost(mGhostDialog->data());
			}
		});
		
		mOriginToolbar->addSeparator();
		auto left = mOriginToolbar->addAction("Cell Left", [&]() {
			mOriginView->moveCellOrigin(-1, 0);
			});
		left->setShortcut(QKeySequence(Qt::Key_A));
		auto right = mOriginToolbar->addAction("Cell Right", [&]() {
			mOriginView->moveCellOrigin(1, 0);
			});
		right->setShortcut(QKeySequence(Qt::Key_D));
		auto up = mOriginToolbar->addAction("Cell Up", [&]() {
			mOriginView->moveCellOrigin(0, -1);
			});
		up->setShortcut(QKeySequence(Qt::Key_W));
		auto down = mOriginToolbar->addAction("Cell Down", [&]() {
			mOriginView->moveCellOrigin(0, 1);
			});
		down->setShortcut(QKeySequence(Qt::Key_S));

		mOriginToolbar->addSeparator();

		auto prevCell = mOriginToolbar->addAction("Prev Cell", [&]() {
			mOriginView->shiftCell(-1);
			});
		prevCell->setShortcut(QKeySequence(Qt::Key_Q));
		auto nextCell = mOriginToolbar->addAction("Next Cell", [&]() {
			mOriginView->shiftCell(1);
			});
		nextCell->setShortcut(QKeySequence(Qt::Key_E));

		mOriginToolbar->addSeparator();
		auto ghost = mOriginToolbar->addAction("Capture Ghost", [&]() {
			mOriginView->captureGhost();
			});
		ghost->setShortcut(QKeySequence(Qt::Key_C));
			

		mOriginView = new OriginView(this);
		mOriginView->setMinimumWidth(750);
		mOriginView->setStage(&mStage);
		mViewLayout->addWidget(mOriginView);

		addToolBarBreak();
		addToolBar(Qt::ToolBarArea::TopToolBarArea, mOriginToolbar);
	}

	void AtlasEditor::setupClipMode() {
		mClipsToolbar = new QToolBar(this);
		QLabel* label = new QLabel("  FPS  ", mClipsToolbar);
		QSpinBox* box = new QSpinBox(mClipsToolbar);
		box->setMaximum(999);

		mClipsToolbar->addAction("Add Clip", [&]() {
			mClipsWidget->addClip();
		});
		auto removeClip = mClipsToolbar->addAction("Remove Clip", [&]() {
			mClipsWidget->removeClip();
			}); removeClip->setShortcut(QKeySequence(Qt::Key_Delete));
		mClipsToolbar->addAction("Add Frame", [&]() {
			mClipsWidget->addFrame();
		});
		
		mClipsToolbar->addSeparator();
		mClipsToolbar->addWidget(label);
		mClipsToolbar->addWidget(box);

		mClipsWidget = new ClipsWidget(this);
		mClipsWidget->setMinimumWidth(750);

		mClipsWidget->view()->setStage(&mStage);
		mClipsWidget->reel()->setStage(&mStage);
		mViewLayout->addWidget(mClipsWidget);

		mClipsTimer = new QTimer(this);
		connect(mClipsTimer, &QTimer::timeout, mClipsWidget, &ClipsWidget::loop);
		mClipsTimer->start(1000.0f / 30.0f);
		box->setValue(30.0f);

		connect(box, &QSpinBox::valueChanged, [&](int value) {
			mClipsTimer->stop();
			mClipsTimer->start(1000.0f / value);
			});

		addToolBar(Qt::ToolBarArea::TopToolBarArea, mClipsToolbar);
	}

	void AtlasEditor::setupActions() {
		connect(ui.actionNewProject, &QAction::triggered, [&]() {
			gGUI.makeNewProject(this);
			assert(gGUI.open());
			init();
		});
		connect(ui.actionSaveProject, &QAction::triggered, [&]() {
			gGUI.saveCurrentProject();
			});
		connect(ui.actionSaveProjectAs, &QAction::triggered, [&]() {
			gGUI.saveCurrentProjectAs(this);
		});
		connect(ui.actionLoadProject, &QAction::triggered, [&]() {
			gGUI.loadCurrentProject(this);
			assert(gGUI.open());
			init();
		});

		mDebugLoad = new QAction(this);
		addAction(mDebugLoad);
		mDebugLoad->setShortcut(QKeySequence(Qt::Key::Key_P));

		connect(mDebugLoad, &QAction::triggered, [&]() {
			gGUI.loadDebugProject();
			assert(gGUI.open());
			init(true);
		});

		mViewActions = new QActionGroup(this);
		mViewActions->addAction(ui.actionCellMode);
		mViewActions->addAction(ui.actionOriginMode);
		mViewActions->addAction(ui.actionClipMode);
		mViewActions->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);

		connect(mViewActions, &QActionGroup::triggered, [&](QAction* action) {
			switch (mViewMode) {
			case ViewMode::CELL:
				mCellToolbar->hide();
				mCellsWidget->hideEditor();
				gMouse.reset();
				break;
			case ViewMode::ORIGIN:
				mOriginToolbar->hide();
				mOriginView->hideEditor();
				gMouse.reset();
				break;
			case ViewMode::CLIP:
				mClipsToolbar->hide();
				mClipsWidget->hideEditor();
				gMouse.reset();
				break;
			}

			if (action == ui.actionCellMode) {
				mViewMode = ViewMode::CELL;
				mCellsWidget->showEditor();
				mCellToolbar->show();
			}
			else if (action == ui.actionOriginMode) {
				mViewMode = ViewMode::ORIGIN;
				mOriginView->showEditor();
				mOriginToolbar->show();
			}
			else if (action == ui.actionClipMode) {
				mViewMode = ViewMode::CLIP;
				mClipsWidget->showEditor();
				mClipsToolbar->show();
			}
			});

		mViewActions->setEnabled(false);
	}

	void AtlasEditor::keyPressEvent(QKeyEvent* e) {
		if (gGUI.open()) {
			switch (mViewMode) {
			case ViewMode::CELL:
				mCellsWidget->onKeyPress(e);
				break;
			case ViewMode::ORIGIN:
				mOriginView->onKeyPress(e);
				break;
			case ViewMode::CLIP:
				mClipsWidget->onKeyPress(e);
				break;
			}
		}
	}

	void AtlasEditor::keyReleaseEvent(QKeyEvent* e) {
		if (gGUI.open()) {
			switch (mViewMode) {
			case ViewMode::CELL:
				mCellsWidget->onKeyRelease(e);
				break;
			case ViewMode::ORIGIN:
				mOriginView->onKeyRelease(e);
				break;
			case ViewMode::CLIP:
				mClipsWidget->onKeyRelease(e);
				break;
			}
		}
	}

}
