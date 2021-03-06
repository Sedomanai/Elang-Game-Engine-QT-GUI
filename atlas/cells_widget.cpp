#include "cells_widget.h"
#include <qmenu.h>
#include <qlineedit.h>

#include "../elqt/color_code.h"

namespace el
{
	CellsWidget::CellsWidget(QWidget* parent) : 
		mState(CellsWidget::SNONE), QElangPaletteWidget(parent), mSuppressSelect(false), mAlphaCut(10), mCtrl(false)
	{
		ui.view->sig_Start.connect([&]() {
			ui.view->makeCurrent();
			connectMouseInput();
			connectObserver<CellHolder>(mSelects);
		});

		ui.view->sig_Paint.connect([&]() {
			if (gGUI.open()) {

				color8 c = selectColoring();
				for (obj<CellHolder> holder : mSelects) {
					c.a = 255;
					mHighlighter->line.batchAABB(holder->rect, c, 0.0f);
					c.a = gEditorColor.cellFillAlpha;
					mHighlighter->fill.batchAABB(holder->rect, c, 0.0f);
				}

				switch (mState) {
				case SELECTING:
					if (mCtrl)
						mHighlighter->line.batchAABB(mSelectRect, gEditorColor.createRect, -10.0f);
					else 
						mHighlighter->line.batchAABB(mSelectRect, gEditorColor.selectRect, -10.0f);
					break;
				}
				mHighlighter->draw();
			}
		});

		connectList();
	}

	void CellsWidget::onKeyPress(QKeyEvent* e) {
		if (e->key() == Qt::Key::Key_Alt) {
			findCursorState();
			ui.view->update();
		}
		if (e->key() == Qt::Key::Key_Control) {
			mCtrl = true;
			findCursorState();
			ui.view->update();
		}
	}

	void CellsWidget::onKeyRelease(QKeyEvent* e) {
		if (e->key() == Qt::Key::Key_Alt) {
			findCursorState();
			ui.view->update();
		}
		if (e->key() == Qt::Key::Key_Control) {
			mCtrl = false;
			findCursorState();
			ui.view->update();
		}
	}

	color8 CellsWidget::selectColoring() {
		if (mTempCursor == Qt::ArrowCursor)
			return gEditorColor.cellHovered;
		if (mTempCursor == Qt::OpenHandCursor)
			return gEditorColor.cellOpenHanded;
		else if (mTempCursor == Qt::ClosedHandCursor)
			return gEditorColor.cellShadow;
		else {
			if (mState == SNONE)
				return gEditorColor.cellSizing;
			else
				return gEditorColor.cellShadow;
		}
	}

	void CellsWidget::autoGenCells(uint sortorder, uint target_margin) {
		if (gGUI.open() && mTexture) {
			//ui.view->bindStage(); //TODO: investigate why this is necessary

			safeClearSelection();
			mTexture->autoGenerateAtlas(mTexture, mAlphaCut);
			sortCells(sortorder, target_margin);
			recreateCellHoldersFromAtlas();
			recreateList();
			redrawAllCellHolders();
			renameAll();
		}
	}

	void CellsWidget::renameAll() {
		auto& cells = *gAtlsUtil.cellList;
		mTexture->atlas->cells.clear();
		auto name = gProject->atlases[mTexture->atlas];

		for (auto i = 0; i < cells.count(); i++) {
			auto rname = name + "_" + std::to_string(i);
			CellItem* it = reinterpret_cast<CellItem*>(cells.item(i));
			it->setText(QString::fromUtf8(rname));
			mTexture->atlas->cells.emplace(rname, it->holder->cell);
		}
	}

	//TODO: create alignment ?
	void CellsWidget::sortCells(uint sortorder, uint target_margin) {
		auto&& cv = mTexture->atlas->linearCells();

		std::sort(cv.begin(), cv.end(), [&](asset<Cell> lhs, asset<Cell> rhs) ->bool {
			bool ret = false;
			float margin;

			auto lhori = lhs->uvUp;
			auto rhori = rhs->uvUp;
			auto lverti = lhs->uvLeft;
			auto rverti = rhs->uvLeft;

			if (sortorder == 0) {
				margin = abs(lhori - rhori) * mTexture->height();
				ret = (margin < target_margin) ? (lverti > rverti) : (lhori > rhori);
			}
			else {
				margin = abs(lverti - rverti) * mTexture->width();
				ret = (margin < target_margin) ? (lhori < rhori) : (lverti > rverti);
			}
			return ret;
		});

		for (sizet i = 0; i < cv.size(); i++)
			cv[i]->index = i;
	}

	void CellsWidget::safeClearSelection() {
		mSuppressSelect = true;
		gAtlsUtil.cellList->clearSelection();
		gAtlsUtil.cellList->setCurrentItem(0);
		mSelects.clear();
		mSuppressSelect = false;
	}

	void CellsWidget::connectMouseInput() {
		ui.view->sig_MousePress.connect([&]() {
			if (gGUI.open() && mTexture && mTexture->atlas && gMouse.state(0) == eInput::ONCE) {
				findCursorState();
				if (QApplication::keyboardModifiers() & Qt::AltModifier) {
					if (mCursorState == MOVE)
						mState = MOVING;
					else if (mCursorState > MOVE) {
						mState = SIZING;
					}
				} else {
					auto pos = *mMainCam * gMouse.currentPosition();
					mState = SELECTING;
					mSelectRect.l = pos.x;
					mSelectRect.r = pos.x;
					mSelectRect.t = pos.y;
					mSelectRect.b = pos.y;
				}

				findCursorState();
				ui.view->update();
			}
		});

		ui.view->sig_MouseMove.connect([&]() {
			if (gGUI.open()) {
				findCursorState();

				if (gMouse.state(0) == eInput::HOLD) {
					auto pos = *mMainCam * gMouse.currentPosition();
					auto delta = *mMainCam * gMouse.deltaPosition();
					delta -= mMainCam->position();

					switch (mState) {
					case SELECTING:
						mSelectRect.r = pos.x;
						mSelectRect.t = pos.y;
						ui.view->update();
						break;

					case MOVING:
						for (obj<CellHolder> holder : mSelects) {
							holder->rect.move(delta);
						}
						ui.view->update();
						break;

					case SIZING:
						if (mSelects.size() == 1) {
							for (obj<CellHolder> holder : mSelects) {
								if ((mCursorState & LEFT) == LEFT) {
									holder->rect.l = pos.x;
								}
								else if ((mCursorState & RIGHT) == RIGHT) {
									holder->rect.r = pos.x;
								}
								if ((mCursorState & BOTTOM) == BOTTOM) {
									holder->rect.b = pos.y;
								}
								else if ((mCursorState & TOP) == TOP) {
									holder->rect.t = pos.y;
								}
							}
							ui.view->update();
						}
						break;
					}
				}
			}
		});

		ui.view->sig_MouseRelease.connect([&]() {
			if (gGUI.open() && gMouse.state(0) == eInput::LIFT) {
				switch (mState) {
				case SIZING:
					if (mSelects.size() == 1) {
						for (obj<CellHolder> holder : mSelects) {
							assert(holder);
							holder->rect.normalize();
							holder->rect.roundCorners();
							holder->moldCellFromRect((int)mTexture->width(), (int)mTexture->height(), holder->cell->index);
							redrawAllCellHolders();
						}
					}
					break;
				case MOVING:
					assert(mSelects.size() > 0);
					for (obj<CellHolder> holder : mSelects) {
						assert(holder);
						holder->rect.roundCorners();
						holder->moldCellFromRect((int)mTexture->width(), (int)mTexture->height(), holder->cell->index);
					} redrawAllCellHolders();
				break;
				case SELECTING:
					if (mCtrl) {
						mSelectRect.normalize();
						mSelectRect.roundCorners();
						createNamedCell();
						redrawAllCellHolders();
					} else {
						mSuppressSelect = true;
						gAtlsUtil.cellList->clearSelection();
						mSelectRect.normalize();

						mSelects.clear();
						for (obj<CellHolder> holder : gStage->view<CellHolder>()) {
							if (holder->rect.intersects(mSelectRect)) {
								holder.update();
								assert(holder.has<CellItem*>());
								holder.get<CellItem*>()->setSelected(true);
							}
						}

						if (mSelects.size() > 0) {
							gAtlsUtil.cellList->setCurrentItem(obj<CellHolder>(mSelects.data()[0]).get<CellItem*>());
						}
						mSuppressSelect = false;
					}
					break;
				}

				mState = SNONE;
				findCursorState();
				ui.view->update();
			}
		});
	}

	void CellsWidget::deleteSelected() {
		assert(mTexture && mTexture->atlas);
		mSuppressSelect = true;
		for (obj<CellHolder> holder : mSelects) {
			deleteCell(holder);
		} 
		mSelects.clear();
		mSuppressSelect = false;
		redrawAllCellHolders();
		ui.view->update();
	}

	void CellsWidget::deleteCell(obj<CellHolder> holder) {
		delete holder.get<CellItem*>();
		mTexture->atlas->cells.erase(holder->cell);
		holder->cell.destroy();
		holder.destroy();
	}

	obj<CellHolder> CellsWidget::createCell(const string& name) {
		assert(mTexture && mTexture->atlas);
		mSelectRect.roundCorners();
		mSelectRect.normalize();
		CellItem* item = new CellItem(gAtlsUtil.cellList);
		auto holder = item->holder = gStage->make<CellHolder>(gProject->makeSub<Cell>(), mSelectRect);
		holder.add<CellItem*>(item);
		holder.add<Button>(this);
		holder->moldCellFromRect((int)mTexture->width(), (int)mTexture->height(), gAtlsUtil.cellList->count());
		mTexture->atlas->cells.emplace(name, holder->cell);
		item->setText(QString::fromUtf8(name));

		mSuppressSelect = true;
		gAtlsUtil.cellList->addItem(item);
		gAtlsUtil.cellList->clearSelection();
		mSelects.clear();
		item->setSelected(true);
		gAtlsUtil.cellList->setCurrentItem(item);
		mSuppressSelect = false;

		holder.update();
		return holder;
	}

	void CellsWidget::createNamedCell() {
		createCell(
			gAtlsUtil.cellList->getNoneConflictingName(
				QString::fromStdString(gProject->atlases[mTexture->atlas]), false
			).toStdString()
		);
	}

	void CellsWidget::autoCreateCell() {
		static vector<int> valids;

		if (mTexture && mTexture->atlas) {
			aabb rect;
			bool made = false;

			vec2 mousepix = (*mMainCam) * gMouse.currentPosition();

			uint pixcount = mTexture->width() * mTexture->height();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mTexture->id());

			auto pixels = (unsigned char*)malloc(pixcount);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_ALPHA, GL_UNSIGNED_BYTE, pixels);

			bool* visited = (bool*)calloc(pixcount, sizeof(bool));

			//TODO: change resolution
			uint reader = 0, rend = 1;
			mousepix.x = round(mousepix.x);
			mousepix.y = round(mousepix.y);
			mousepix.y *= -1;
			if (0 < mousepix.x &&
				0 < mousepix.y &&
				mousepix.x < mTexture->width() &&
				mousepix.y < mTexture->height()) {
				int start = mousepix.x + mousepix.y * mTexture->width();
				if (pixels[start] > mAlphaCut) {
					valids.clear();
					valids.push_back(start);
					visited[start] = true;

					while (true) {
						rend = valids.size();
						for (; reader < rend; reader++) {
							auto curr = valids[reader];
							bool notop =
								((curr < mTexture->width()) || (pixels[curr - mTexture->width()] <= mAlphaCut) || visited[curr - mTexture->width()] == true);
							bool nobottom =
								(curr > (mTexture->width() * (mTexture->height() - 1)) || (pixels[curr + mTexture->width()] <= mAlphaCut) || (visited[curr + mTexture->width()] == true));
							bool noleft =
								((curr % mTexture->width() == 0) || (pixels[curr - 1] <= mAlphaCut) || (visited[curr - 1] == true));
							bool noright =
								((curr % mTexture->width() == (mTexture->width() - 1)) || (pixels[curr + 1] <= mAlphaCut) || (visited[curr + 1] == true));
							/*bool notopleft =
								((curr < mTexWidth) || (pixels[curr - mTexWidth] <= 25) || visited[curr - mTexWidth] == true);
							bool notopright =
								(curr > (mTexWidth * (mTexHeight - 1)) || (pixels[curr + mTexWidth] <= 25) || (visited[curr + mTexWidth] == true));
							bool nobottomleft =
								((curr % mTexWidth == 0) || (pixels[curr - 1] <= 25) || (visited[curr - 1] == true));
							bool nobottomright =
								((curr % mTexWidth == (mTexWidth - 1)) || (pixels[curr + 1] <= 25) || (visited[curr + 1] == true));*/

							if (!notop) {
								valids.push_back(curr - mTexture->width());
								visited[curr - mTexture->width()] = true;
							} if (!nobottom) {
								valids.push_back(curr + mTexture->width());
								visited[curr + mTexture->width()] = true;
							} if (!noleft) {
								valids.push_back(curr - 1);
								visited[curr - 1] = true;
							} if (!noright) {
								valids.push_back(curr + 1);
								visited[curr + 1] = true;
							}
						}

						if (rend == valids.size())
							break;
					}

					auto intmax = std::numeric_limits<int>::max();
					int il = intmax;
					int ib = intmax;
					int ir = -intmax;
					int it = -intmax;
					for (uint i = 0; i < valids.size(); i++) {
						auto& s = valids[i];
						il = min(il, (s % (int)mTexture->width()));
						ib = min(ib, (s / (int)mTexture->width()));
						ir = max(ir, (s % (int)mTexture->width()) + 1);
						it = max(it, (s / (int)mTexture->width()) + 1);
					}

					rect = aabb(il, -it, ir, -ib);
					made = true;
				}
			}
			free(pixels);

			if (made) {
				mSelectRect = Box(rect.l, rect.b, rect.r, rect.t);
				createNamedCell();
				redrawAllCellHolders();
				ui.view->update();
			}

			//recreateList();
		}
	}

	void CellsWidget::connectList() {
		connect(gAtlsUtil.cellList, &QListExtension::itemSelectionChanged, [&]() {
			if (!gGUI.open())
				return;

			if (isVisible() && !mSuppressSelect) {
				assert(mTexture && mTexture->atlas);
				mSelects.clear();
				for (auto i = 0; i < gAtlsUtil.cellList->count(); i++) {
					CellItem* item = reinterpret_cast<CellItem*>(gAtlsUtil.cellList->item(i));
					if (item->isSelected())
						item->holder.update();
				}

				ui.view->update();
			}
		});

		connect(gAtlsUtil.cellList->itemDelegate(), &QAbstractItemDelegate::commitData, [&](QWidget* pLineEdit) {
			if (isVisible()) {
				auto name = gAtlsUtil.cellList->
					getNoneConflictingName(reinterpret_cast<QLineEdit*>(pLineEdit)->text()).toStdString();
				CellItem* item = reinterpret_cast<CellItem*>(gAtlsUtil.cellList->item(gAtlsUtil.cellList->currentRow()));
				assert(mTexture && mTexture->atlas);
				mTexture->atlas->cells.erase(item->text().toStdString());
				mTexture->atlas->cells.emplace(name, item->holder->cell);
				item->setText(QString::fromStdString(name));
			}
		});
	}

	void CellsWidget::combineCells() {
		assert(mTexture && mTexture->atlas);
		if (mSelects.size() > 1) {
			string name = obj<CellHolder>(mSelects.data()[0]).get<CellItem*>()->text().toStdString();
			int l = std::numeric_limits<int>::max();
			int b = std::numeric_limits<int>::max();
			int r = std::numeric_limits<int>::min();
			int t = std::numeric_limits<int>::min();
			mSuppressSelect = true;
			gAtlsUtil.cellList->clearSelection();
			sizet i = 0;
			for (obj<CellHolder> holder : mSelects) {
				l = min(l, holder->rect.l);
				b = min(b, holder->rect.b);
				r = max(r, holder->rect.r);
				t = max(t, holder->rect.t);
				if (i != 0)
					deleteCell(holder);
				i++;
			}
			mSuppressSelect = false;
			auto first = obj<CellHolder>(mSelects.data()[0]);
			first->rect = Box(l, b, r, t);
			first->moldCellFromRect(mTexture->width(), mTexture->height(), first->cell->index);
			mSelects.clear();
			gAtlsUtil.cellList->setCurrentItem(first.get<CellItem*>());
			redrawAllCellHolders();
			ui.view->update();
		}
	}


	/*

	void CellsWidget::connectList() {
		connect(gCellList, &QWidget::customContextMenuRequested, [=](const QPoint& pos) {
			if (isVisible()) {
				QMenu contextMenu(tr("Context menu"), this);
				auto CREATE_ACT = contextMenu.addAction("Create");
				auto DELETE_ACT = contextMenu.addAction("Delete");

				auto selected = contextMenu.exec(gCellList->mapToGlobal(pos));
				if (selected) {
					if (selected == CREATE_ACT) {

					} else if (selected == DELETE_ACT) {

					}
				}
			}
		});
	}
	void CellsWidget::importAtlasBegin() {
		deleteAll();
	}

	void CellsWidget::importAtlasEnd() {
		recreateList();
		update();
	}

	/**/


	void CellsWidget::onTextureUpdate() {
		assert(mTexture);
		safeClearSelection();
		recreateCellHoldersFromAtlas();
		recreateList();
		redrawAllCellHolders();
	}

	void CellsWidget::recreateList() {
		assert(gGUI.open());

		auto& list = *gAtlsUtil.cellList;
		for (auto i = 0; i < list.count(); i++) {
			delete list.item(i);
		} list.clear();

		if (mTexture && mTexture->atlas) {
			auto& cells = mTexture->atlas->cells;
			mSuppressSelect = true;
			for (obj<CellHolder> holder : gStage->view<CellHolder>()) {
				CellItem* item = new CellItem(&list);
				item->holder = holder;
				assert(cells.contains(holder->cell));
				item->setText(QString::fromUtf8(cells[holder->cell]));
				holder.add<CellItem*>(item);
				list.addItem(item);
			}
			mSuppressSelect = false;
		}
	}

	void CellsWidget::showEditor() {
		auto list = gAtlsUtil.cellList;
		list->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
		list->setDragDropMode(QAbstractItemView::DragDropMode::DragDrop);
		list->setDefaultDropAction(Qt::DropAction::MoveAction);
		list->show();
		show();
	}

	void CellsWidget::hideEditor() {
		//mSelects.clear();
		//gAtlsUtil.cellList->clearSelection();
		gAtlsUtil.cellList->hide();
		hide();
	}

	void CellsWidget::findCursorState() {
		assert(gGUI.open() && mTexture);
		auto pos = *mMainCam * gMouse.currentPosition();
		if (!mTexture->atlas) {
			setCursor(Qt::ArrowCursor);
			mCursorState = CursorState::NONE;
			return;
		}

		if (mState == SNONE) {
			mCursorState = CursorState::NONE;

			bool hit = false; // = mSelects.size() > 0;
			for (obj<CellHolder> holder : mSelects) {
				if (holder->rect.contains(pos)); {
					hit = true;
					break;
				}
			}

			if (hit) {
				auto sx = mMainCam->scale().x;
				if (mSelects.size() == 1) {
					auto& rect = obj<CellHolder>(mSelects.data()[0])->rect;
					if (rect.l > pos.x - 5.0f * sx)
						mCursorState += CursorState::LEFT;
					else if (rect.r < pos.x + 5.0f * sx)
						mCursorState += CursorState::RIGHT;
					if (rect.b > pos.y - 5.0f * sx)
						mCursorState += CursorState::BOTTOM;
					else if (rect.t < pos.y + 5.0f * sx)
						mCursorState += CursorState::TOP;
				}

				if (mCursorState == 0)
					mCursorState = CursorState::MOVE;
			}

			if (QApplication::queryKeyboardModifiers() & Qt::AltModifier) {
				mTempCursor = Qt::ArrowCursor;
				if (mCursorState == CursorState::MOVE)
					mTempCursor = Qt::OpenHandCursor;

				bool left = ((mCursorState & CursorState::LEFT) == CursorState::LEFT);
				bool right = ((mCursorState & CursorState::RIGHT) == CursorState::RIGHT);
				bool top = ((mCursorState & CursorState::TOP) == CursorState::TOP);
				bool bottom = ((mCursorState & CursorState::BOTTOM) == CursorState::BOTTOM);

				if (left || right) {
					mTempCursor = Qt::SizeHorCursor;
				}
				if (top || bottom) {
					if (mTempCursor == Qt::SizeHorCursor) {
						if ((left && top) || (right && bottom)) {
							mTempCursor = Qt::SizeFDiagCursor;
						} else mTempCursor = Qt::SizeBDiagCursor;
					} else
						mTempCursor = Qt::SizeVerCursor;
				}
			} else mTempCursor = Qt::ArrowCursor;
		}	
		
		if (mState == MOVING) {
			mTempCursor = Qt::ClosedHandCursor;
		} 

		if (mTempCursor == Qt::ArrowCursor && mCtrl) {
			mTempCursor == Qt::CrossCursor;
		}

		
		setCursor(mTempCursor);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////

}