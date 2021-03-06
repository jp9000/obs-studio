#pragma once

#include <obs.hpp>
#include <graphics/vec2.h>
#include <graphics/matrix4.h>
#include <util/threading.h>
#include <mutex>
#include <vector>
#include "qt-display.hpp"
#include "obs-app.hpp"

class OBSBasic;
class QMouseEvent;

#define ITEM_LEFT (1 << 0)
#define ITEM_RIGHT (1 << 1)
#define ITEM_TOP (1 << 2)
#define ITEM_BOTTOM (1 << 3)

#define ZOOM_SENSITIVITY 1.125f

enum class ItemHandle : uint32_t {
	None = 0,
	TopLeft = ITEM_TOP | ITEM_LEFT,
	TopCenter = ITEM_TOP,
	TopRight = ITEM_TOP | ITEM_RIGHT,
	CenterLeft = ITEM_LEFT,
	CenterRight = ITEM_RIGHT,
	BottomLeft = ITEM_BOTTOM | ITEM_LEFT,
	BottomCenter = ITEM_BOTTOM,
	BottomRight = ITEM_BOTTOM | ITEM_RIGHT,
};

class OBSBasicPreview : public OBSQTDisplay {
	Q_OBJECT

	friend class SourceTree;
	friend class SourceTreeItem;

private:
	obs_sceneitem_crop startCrop;
	vec2 startItemPos;
	vec2 cropSize;
	OBSSceneItem stretchGroup;
	OBSSceneItem stretchItem;
	ItemHandle stretchHandle = ItemHandle::None;
	vec2 stretchItemSize;
	matrix4 screenToItem;
	matrix4 itemToScreen;
	matrix4 invGroupTransform;

	gs_texture_t *overflow = nullptr;
	gs_vertbuffer_t *rectFill = nullptr;

	vec2 startPos;
	vec2 mousePos;
	vec2 lastMoveOffset;
	vec2 scrollingFrom;
	vec2 scrollingOffset;
	bool mouseDown = false;
	bool mouseMoved = false;
	bool mouseOverItems = false;
	bool cropping = false;
	bool locked = false;
	bool scrollMode = false;
	bool fixedScaling = false;
	bool selectionBox = false;
	int32_t scalingLevel = 0;
	float scalingAmount = 1.0f;

	std::vector<obs_sceneitem_t *> hoveredPreviewItems;
	std::vector<obs_sceneitem_t *> selectedItems;
	std::mutex selectMutex;

	static vec2 GetMouseEventPos(QMouseEvent *event);
	static bool FindSelected(obs_scene_t *scene, obs_sceneitem_t *item,
				 void *param);
	static bool DrawSelectedOverflow(obs_scene_t *scene,
					 obs_sceneitem_t *item, void *param);
	static bool DrawSelectedItem(obs_scene_t *scene, obs_sceneitem_t *item,
				     void *param);
	static bool DrawSelectionBox(float x1, float y1, float x2, float y2,
				     gs_vertbuffer_t *box);

	static OBSSceneItem GetItemAtPos(const vec2 &pos, bool selectBelow);
	static bool SelectedAtPos(const vec2 &pos);

	static void DoSelect(const vec2 &pos);
	static void DoCtrlSelect(const vec2 &pos);

	static vec3 GetSnapOffset(const vec3 &tl, const vec3 &br);

	void GetStretchHandleData(const vec2 &pos);

	void UpdateCursor(uint32_t &flags);

	void SnapStretchingToScreen(vec3 &tl, vec3 &br);
	void ClampAspect(vec3 &tl, vec3 &br, vec2 &size, const vec2 &baseSize);
	vec3 CalculateStretchPos(const vec3 &tl, const vec3 &br);
	void CropItem(const vec2 &pos);
	void StretchItem(const vec2 &pos);

	static void SnapItemMovement(vec2 &offset);
	void MoveItems(const vec2 &pos);
	void BoxItems(const vec2 &startPos, const vec2 &pos);

	void ProcessClick(const vec2 &pos);

public:
	OBSBasicPreview(QWidget *parent,
			Qt::WindowFlags flags = Qt::WindowFlags());
	~OBSBasicPreview();

	static OBSBasicPreview *Get();

	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void keyReleaseEvent(QKeyEvent *event) override;

	virtual void wheelEvent(QWheelEvent *event) override;

	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void leaveEvent(QEvent *event) override;

	void DrawOverflow();
	void DrawSceneEditing();

	inline void SetLocked(bool newLockedVal) { locked = newLockedVal; }
	inline void ToggleLocked() { locked = !locked; }
	inline bool Locked() const { return locked; }

	inline void SetFixedScaling(bool newFixedScalingVal)
	{
		fixedScaling = newFixedScalingVal;
	}
	inline bool IsFixedScaling() const { return fixedScaling; }

	void SetScalingLevel(int32_t newScalingLevelVal);
	void SetScalingAmount(float newScalingAmountVal);
	inline int32_t GetScalingLevel() const { return scalingLevel; }
	inline float GetScalingAmount() const { return scalingAmount; }

	void ResetScrollingOffset();
	inline void SetScrollingOffset(float x, float y)
	{
		vec2_set(&scrollingOffset, x, y);
	}
	inline float GetScrollX() const { return scrollingOffset.x; }
	inline float GetScrollY() const { return scrollingOffset.y; }

	/* use libobs allocator for alignment because the matrices itemToScreen
	 * and screenToItem may contain SSE data, which will cause SSE
	 * instructions to crash if the data is not aligned to at least a 16
	 * byte boundary. */
	static inline void *operator new(size_t size) { return bmalloc(size); }
	static inline void operator delete(void *ptr) { bfree(ptr); }
};
