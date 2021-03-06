#include <QGuiApplication>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <string>
#include <graphics/vec4.h>
#include <graphics/matrix4.h>
#include "window-basic-preview.hpp"
#include "window-basic-main.hpp"
#include "obs-app.hpp"
#include "platform.hpp"

#define HANDLE_RADIUS 4.0f
#define HANDLE_SEL_RADIUS (HANDLE_RADIUS * 1.5f)
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
#define SUPPORTS_FRACTIONAL_SCALING
#endif

/* TODO: make C++ math classes and clean up code here later */

OBSBasicPreview::OBSBasicPreview(QWidget *parent, Qt::WindowFlags flags)
	: OBSQTDisplay(parent, flags)
{
	ResetScrollingOffset();
	setMouseTracking(true);
}

OBSBasicPreview::~OBSBasicPreview()
{
	obs_enter_graphics();

	if (overflow)
		gs_texture_destroy(overflow);
	if (rectFill)
		gs_vertexbuffer_destroy(rectFill);

	obs_leave_graphics();
}

vec2 OBSBasicPreview::GetMouseEventPos(QMouseEvent *event)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
#ifdef SUPPORTS_FRACTIONAL_SCALING
	float pixelRatio = main->devicePixelRatioF();
#else
	float pixelRatio = main->devicePixelRatio();
#endif
	float scale = pixelRatio / main->previewScale;
	vec2 pos;
	vec2_set(&pos,
		 (float(event->x()) - main->previewX / pixelRatio) * scale,
		 (float(event->y()) - main->previewY / pixelRatio) * scale);

	return pos;
}

struct SceneFindData {
	const vec2 &pos;
	OBSSceneItem item;
	bool selectBelow;

	obs_sceneitem_t *group = nullptr;

	SceneFindData(const SceneFindData &) = delete;
	SceneFindData(SceneFindData &&) = delete;
	SceneFindData &operator=(const SceneFindData &) = delete;
	SceneFindData &operator=(SceneFindData &&) = delete;

	inline SceneFindData(const vec2 &pos_, bool selectBelow_)
		: pos(pos_), selectBelow(selectBelow_)
	{
	}
};

struct SceneFindBoxData {
	const vec2 &startPos;
	const vec2 &pos;
	std::vector<obs_sceneitem_t *> sceneItems;

	SceneFindBoxData(const SceneFindData &) = delete;
	SceneFindBoxData(SceneFindData &&) = delete;
	SceneFindBoxData &operator=(const SceneFindData &) = delete;
	SceneFindBoxData &operator=(SceneFindData &&) = delete;

	inline SceneFindBoxData(const vec2 &startPos_, const vec2 &pos_)
		: startPos(startPos_), pos(pos_)
	{
	}
};

static bool SceneItemHasVideo(obs_sceneitem_t *item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_VIDEO) != 0;
}

static bool CloseFloat(float a, float b, float epsilon = 0.01)
{
	using std::abs;
	return abs(a - b) <= epsilon;
}

static bool FindItemAtPos(obs_scene_t *scene, obs_sceneitem_t *item,
			  void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData *>(param);
	matrix4 transform;
	matrix4 invTransform;
	vec3 transformedPos;
	vec3 pos3;
	vec3 pos3_;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	    transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (data->selectBelow && obs_sceneitem_selected(item)) {
			if (data->item)
				return false;
			else
				data->selectBelow = false;
		}

		data->item = item;
	}

	UNUSED_PARAMETER(scene);
	return true;
}

static vec3 GetTransformedPos(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}

static inline vec2 GetOBSScreenSize()
{
	obs_video_info ovi;
	vec2 size;
	vec2_zero(&size);

	if (obs_get_video_info(&ovi)) {
		size.x = float(ovi.base_width);
		size.y = float(ovi.base_height);
	}

	return size;
}

vec3 OBSBasicPreview::GetSnapOffset(const vec3 &tl, const vec3 &br)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	vec2 screenSize = GetOBSScreenSize();
	vec3 clampOffset;

	vec3_zero(&clampOffset);

	const bool snap = config_get_bool(GetGlobalConfig(), "BasicWindow",
					  "SnappingEnabled");
	if (snap == false)
		return clampOffset;

	const bool screenSnap = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "ScreenSnapping");
	const bool centerSnap = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "CenterSnapping");

	const float clampDist = config_get_double(GetGlobalConfig(),
						  "BasicWindow",
						  "SnapDistance") /
				main->previewScale;
	const float centerX = br.x - (br.x - tl.x) / 2.0f;
	const float centerY = br.y - (br.y - tl.y) / 2.0f;

	// Left screen edge.
	if (screenSnap && fabsf(tl.x) < clampDist)
		clampOffset.x = -tl.x;
	// Right screen edge.
	if (screenSnap && fabsf(clampOffset.x) < EPSILON &&
	    fabsf(screenSize.x - br.x) < clampDist)
		clampOffset.x = screenSize.x - br.x;
	// Horizontal center.
	if (centerSnap && fabsf(screenSize.x - (br.x - tl.x)) > clampDist &&
	    fabsf(screenSize.x / 2.0f - centerX) < clampDist)
		clampOffset.x = screenSize.x / 2.0f - centerX;

	// Top screen edge.
	if (screenSnap && fabsf(tl.y) < clampDist)
		clampOffset.y = -tl.y;
	// Bottom screen edge.
	if (screenSnap && fabsf(clampOffset.y) < EPSILON &&
	    fabsf(screenSize.y - br.y) < clampDist)
		clampOffset.y = screenSize.y - br.y;
	// Vertical center.
	if (centerSnap && fabsf(screenSize.y - (br.y - tl.y)) > clampDist &&
	    fabsf(screenSize.y / 2.0f - centerY) < clampDist)
		clampOffset.y = screenSize.y / 2.0f - centerY;

	return clampOffset;
}

OBSSceneItem OBSBasicPreview::GetItemAtPos(const vec2 &pos, bool selectBelow)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return OBSSceneItem();

	SceneFindData data(pos, selectBelow);
	obs_scene_enum_items(scene, FindItemAtPos, &data);
	return data.item;
}

static bool CheckItemSelected(obs_scene_t *scene, obs_sceneitem_t *item,
			      void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData *>(param);
	matrix4 transform;
	vec3 transformedPos;
	vec3 pos3;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_is_group(item)) {
		data->group = item;
		obs_sceneitem_group_enum_items(item, CheckItemSelected, param);
		data->group = nullptr;

		if (data->item) {
			return false;
		}
	}

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	if (data->group) {
		matrix4 parent_transform;
		obs_sceneitem_get_draw_transform(data->group,
						 &parent_transform);
		matrix4_mul(&transform, &transform, &parent_transform);
	}

	matrix4_inv(&transform, &transform);
	vec3_transform(&transformedPos, &pos3, &transform);

	if (transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (obs_sceneitem_selected(item)) {
			data->item = item;
			return false;
		}
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool OBSBasicPreview::SelectedAtPos(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return false;

	SceneFindData data(pos, false);
	obs_scene_enum_items(scene, CheckItemSelected, &data);
	return !!data.item;
}

struct HandleFindData {
	const vec2 &pos;
	const float radius;
	matrix4 parent_xform;

	OBSSceneItem item;
	ItemHandle handle = ItemHandle::None;

	HandleFindData(const HandleFindData &) = delete;
	HandleFindData(HandleFindData &&) = delete;
	HandleFindData &operator=(const HandleFindData &) = delete;
	HandleFindData &operator=(HandleFindData &&) = delete;

	inline HandleFindData(const vec2 &pos_, float scale)
		: pos(pos_), radius(HANDLE_SEL_RADIUS / scale)
	{
		matrix4_identity(&parent_xform);
	}

	inline HandleFindData(const HandleFindData &hfd,
			      obs_sceneitem_t *parent)
		: pos(hfd.pos),
		  radius(hfd.radius),
		  item(hfd.item),
		  handle(hfd.handle)
	{
		obs_sceneitem_get_draw_transform(parent, &parent_xform);
	}
};

static bool FindHandleAtPos(obs_scene_t *scene, obs_sceneitem_t *item,
			    void *param)
{
	HandleFindData &data = *reinterpret_cast<HandleFindData *>(param);

	if (!obs_sceneitem_selected(item)) {
		if (obs_sceneitem_is_group(item)) {
			HandleFindData newData(data, item);
			obs_sceneitem_group_enum_items(item, FindHandleAtPos,
						       &newData);
			data.item = newData.item;
			data.handle = newData.handle;
		}

		return true;
	}

	matrix4 transform;
	vec3 pos3;
	float closestHandle = data.radius;

	vec3_set(&pos3, data.pos.x, data.pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	auto TestHandle = [&](float x, float y, ItemHandle handle) {
		vec3 handlePos = GetTransformedPos(x, y, transform);
		vec3_transform(&handlePos, &handlePos, &data.parent_xform);

		float dist = vec3_dist(&handlePos, &pos3);
		if (dist < data.radius) {
			if (dist < closestHandle) {
				closestHandle = dist;
				data.handle = handle;
				data.item = item;
			}
		}
	};

	TestHandle(0.0f, 0.0f, ItemHandle::TopLeft);
	TestHandle(0.5f, 0.0f, ItemHandle::TopCenter);
	TestHandle(1.0f, 0.0f, ItemHandle::TopRight);
	TestHandle(0.0f, 0.5f, ItemHandle::CenterLeft);
	TestHandle(1.0f, 0.5f, ItemHandle::CenterRight);
	TestHandle(0.0f, 1.0f, ItemHandle::BottomLeft);
	TestHandle(0.5f, 1.0f, ItemHandle::BottomCenter);
	TestHandle(1.0f, 1.0f, ItemHandle::BottomRight);

	UNUSED_PARAMETER(scene);
	return true;
}

static vec2 GetItemSize(obs_sceneitem_t *item)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(item);
	vec2 size;

	if (boundsType != OBS_BOUNDS_NONE) {
		obs_sceneitem_get_bounds(item, &size);
	} else {
		obs_source_t *source = obs_sceneitem_get_source(item);
		obs_sceneitem_crop crop;
		vec2 scale;

		obs_sceneitem_get_scale(item, &scale);
		obs_sceneitem_get_crop(item, &crop);
		size.x = float(obs_source_get_width(source) - crop.left -
			       crop.right) *
			 scale.x;
		size.y = float(obs_source_get_height(source) - crop.top -
			       crop.bottom) *
			 scale.y;
	}

	return size;
}

void OBSBasicPreview::GetStretchHandleData(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

#ifdef SUPPORTS_FRACTIONAL_SCALING
	float scale = main->previewScale / main->devicePixelRatioF();
#else
	float scale = main->previewScale / main->devicePixelRatio();
#endif
	vec2 scaled_pos = pos;
	vec2_divf(&scaled_pos, &scaled_pos, scale);
	HandleFindData data(scaled_pos, scale);
	obs_scene_enum_items(scene, FindHandleAtPos, &data);

	stretchItem = std::move(data.item);
	stretchHandle = data.handle;

	if (stretchHandle != ItemHandle::None) {
		matrix4 boxTransform;
		vec3 itemUL;
		float itemRot;

		stretchItemSize = GetItemSize(stretchItem);

		obs_sceneitem_get_box_transform(stretchItem, &boxTransform);
		itemRot = obs_sceneitem_get_rot(stretchItem);
		vec3_from_vec4(&itemUL, &boxTransform.t);

		/* build the item space conversion matrices */
		matrix4_identity(&itemToScreen);
		matrix4_rotate_aa4f(&itemToScreen, &itemToScreen, 0.0f, 0.0f,
				    1.0f, RAD(itemRot));
		matrix4_translate3f(&itemToScreen, &itemToScreen, itemUL.x,
				    itemUL.y, 0.0f);

		matrix4_identity(&screenToItem);
		matrix4_translate3f(&screenToItem, &screenToItem, -itemUL.x,
				    -itemUL.y, 0.0f);
		matrix4_rotate_aa4f(&screenToItem, &screenToItem, 0.0f, 0.0f,
				    1.0f, RAD(-itemRot));

		obs_sceneitem_get_crop(stretchItem, &startCrop);
		obs_sceneitem_get_pos(stretchItem, &startItemPos);

		obs_source_t *source = obs_sceneitem_get_source(stretchItem);
		cropSize.x = float(obs_source_get_width(source) -
				   startCrop.left - startCrop.right);
		cropSize.y = float(obs_source_get_height(source) -
				   startCrop.top - startCrop.bottom);

		stretchGroup = obs_sceneitem_get_group(scene, stretchItem);
		if (stretchGroup) {
			obs_sceneitem_get_draw_transform(stretchGroup,
							 &invGroupTransform);
			matrix4_inv(&invGroupTransform, &invGroupTransform);
			obs_sceneitem_defer_group_resize_begin(stretchGroup);
		}
	}
}

void OBSBasicPreview::keyPressEvent(QKeyEvent *event)
{
	if (!IsFixedScaling() || event->isAutoRepeat()) {
		OBSQTDisplay::keyPressEvent(event);
		return;
	}

	switch (event->key()) {
	case Qt::Key_Space:
		setCursor(Qt::OpenHandCursor);
		scrollMode = true;
		break;
	}

	OBSQTDisplay::keyPressEvent(event);
}

void OBSBasicPreview::keyReleaseEvent(QKeyEvent *event)
{
	if (event->isAutoRepeat()) {
		OBSQTDisplay::keyReleaseEvent(event);
		return;
	}

	switch (event->key()) {
	case Qt::Key_Space:
		scrollMode = false;
		setCursor(Qt::ArrowCursor);
		break;
	}

	OBSQTDisplay::keyReleaseEvent(event);
}

void OBSBasicPreview::wheelEvent(QWheelEvent *event)
{
	if (scrollMode && IsFixedScaling()) {
		const int delta = event->angleDelta().y();
		if (delta != 0) {
			if (delta > 0)
				SetScalingLevel(scalingLevel + 1);
			else
				SetScalingLevel(scalingLevel - 1);
			emit DisplayResized();
		}
	}

	OBSQTDisplay::wheelEvent(event);
}

void OBSBasicPreview::mousePressEvent(QMouseEvent *event)
{
	if (scrollMode && IsFixedScaling() &&
	    event->button() == Qt::LeftButton) {
		setCursor(Qt::ClosedHandCursor);
		scrollingFrom.x = event->x();
		scrollingFrom.y = event->y();
		return;
	}

	if (event->button() == Qt::RightButton) {
		scrollMode = false;
		setCursor(Qt::ArrowCursor);
	}

	if (locked) {
		OBSQTDisplay::mousePressEvent(event);
		return;
	}

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
#ifdef SUPPORTS_FRACTIONAL_SCALING
	float pixelRatio = main->devicePixelRatioF();
#else
	float pixelRatio = main->devicePixelRatio();
#endif
	float x = float(event->x()) - main->previewX / pixelRatio;
	float y = float(event->y()) - main->previewY / pixelRatio;
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	bool altDown = (modifiers & Qt::AltModifier);
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	bool ctrlDown = (modifiers & Qt::ControlModifier);

	OBSQTDisplay::mousePressEvent(event);

	if (event->button() != Qt::LeftButton &&
	    event->button() != Qt::RightButton)
		return;

	if (event->button() == Qt::LeftButton)
		mouseDown = true;

	{
		std::lock_guard<std::mutex> lock(selectMutex);
		selectedItems.clear();
	}

	if (altDown)
		cropping = true;

	if (altDown || shiftDown || ctrlDown) {
		vec2 s;
		SceneFindBoxData data(s, s);

		obs_scene_enum_items(main->GetCurrentScene(), FindSelected,
				     &data);

		std::lock_guard<std::mutex> lock(selectMutex);
		selectedItems = data.sceneItems;
	}

	vec2_set(&startPos, x, y);
	GetStretchHandleData(startPos);

	vec2_divf(&startPos, &startPos, main->previewScale / pixelRatio);
	startPos.x = std::round(startPos.x);
	startPos.y = std::round(startPos.y);

	mouseOverItems = SelectedAtPos(startPos);
	vec2_zero(&lastMoveOffset);

	mousePos = startPos;
}

void OBSBasicPreview::UpdateCursor(uint32_t &flags)
{
	if (!flags && cursor().shape() != Qt::OpenHandCursor)
		unsetCursor();
	if (cursor().shape() != Qt::ArrowCursor)
		return;

	if ((flags & ITEM_LEFT && flags & ITEM_TOP) ||
	    (flags & ITEM_RIGHT && flags & ITEM_BOTTOM))
		setCursor(Qt::SizeFDiagCursor);
	else if ((flags & ITEM_LEFT && flags & ITEM_BOTTOM) ||
		 (flags & ITEM_RIGHT && flags & ITEM_TOP))
		setCursor(Qt::SizeBDiagCursor);
	else if (flags & ITEM_LEFT || flags & ITEM_RIGHT)
		setCursor(Qt::SizeHorCursor);
	else if (flags & ITEM_TOP || flags & ITEM_BOTTOM)
		setCursor(Qt::SizeVerCursor);
}

static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem =
		reinterpret_cast<obs_sceneitem_t *>(param);
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, select_one, param);

	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::DoSelect(const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	OBSSceneItem item = GetItemAtPos(pos, true);

	obs_scene_enum_items(scene, select_one, (obs_sceneitem_t *)item);
}

void OBSBasicPreview::DoCtrlSelect(const vec2 &pos)
{
	OBSSceneItem item = GetItemAtPos(pos, false);
	if (!item)
		return;

	bool selected = obs_sceneitem_selected(item);
	obs_sceneitem_select(item, !selected);
}

void OBSBasicPreview::ProcessClick(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();

	if (modifiers & Qt::ControlModifier)
		DoCtrlSelect(pos);
	else
		DoSelect(pos);
}

void OBSBasicPreview::mouseReleaseEvent(QMouseEvent *event)
{
	if (scrollMode)
		setCursor(Qt::OpenHandCursor);

	if (locked) {
		OBSQTDisplay::mouseReleaseEvent(event);
		return;
	}

	if (mouseDown) {
		vec2 pos = GetMouseEventPos(event);

		if (!mouseMoved)
			ProcessClick(pos);

		if (selectionBox) {
			Qt::KeyboardModifiers modifiers =
				QGuiApplication::keyboardModifiers();

			bool altDown = modifiers & Qt::AltModifier;
			bool shiftDown = modifiers & Qt::ShiftModifier;
			bool ctrlDown = modifiers & Qt::ControlModifier;

			std::lock_guard<std::mutex> lock(selectMutex);
			if (altDown || ctrlDown || shiftDown) {
				for (size_t i = 0; i < selectedItems.size();
				     i++) {
					obs_sceneitem_select(selectedItems[i],
							     true);
				}
			}

			for (size_t i = 0; i < hoveredPreviewItems.size();
			     i++) {
				bool select = true;
				obs_sceneitem_t *item = hoveredPreviewItems[i];

				if (altDown) {
					select = false;
				} else if (ctrlDown) {
					select = !obs_sceneitem_selected(item);
				}

				obs_sceneitem_select(hoveredPreviewItems[i],
						     select);
			}
		}

		if (stretchGroup) {
			obs_sceneitem_defer_group_resize_end(stretchGroup);
		}

		stretchItem = nullptr;
		stretchGroup = nullptr;
		mouseDown = false;
		mouseMoved = false;
		cropping = false;
		selectionBox = false;
		unsetCursor();

		OBSSceneItem item = GetItemAtPos(pos, true);

		std::lock_guard<std::mutex> lock(selectMutex);
		hoveredPreviewItems.clear();
		hoveredPreviewItems.push_back(item);
		selectedItems.clear();
	}
}

struct SelectedItemBounds {
	bool first = true;
	vec3 tl, br;
};

static bool AddItemBounds(obs_scene_t *scene, obs_sceneitem_t *item,
			  void *param)
{
	SelectedItemBounds *data =
		reinterpret_cast<SelectedItemBounds *>(param);
	vec3 t[4];

	auto add_bounds = [data, &t]() {
		for (const vec3 &v : t) {
			if (data->first) {
				vec3_copy(&data->tl, &v);
				vec3_copy(&data->br, &v);
				data->first = false;
			} else {
				vec3_min(&data->tl, &data->tl, &v);
				vec3_max(&data->br, &data->br, &v);
			}
		}
	};

	if (obs_sceneitem_is_group(item)) {
		SelectedItemBounds sib;
		obs_sceneitem_group_enum_items(item, AddItemBounds, &sib);

		if (!sib.first) {
			matrix4 xform;
			obs_sceneitem_get_draw_transform(item, &xform);

			vec3_set(&t[0], sib.tl.x, sib.tl.y, 0.0f);
			vec3_set(&t[1], sib.tl.x, sib.br.y, 0.0f);
			vec3_set(&t[2], sib.br.x, sib.tl.y, 0.0f);
			vec3_set(&t[3], sib.br.x, sib.br.y, 0.0f);
			vec3_transform(&t[0], &t[0], &xform);
			vec3_transform(&t[1], &t[1], &xform);
			vec3_transform(&t[2], &t[2], &xform);
			vec3_transform(&t[3], &t[3], &xform);
			add_bounds();
		}
	}
	if (!obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	t[0] = GetTransformedPos(0.0f, 0.0f, boxTransform);
	t[1] = GetTransformedPos(1.0f, 0.0f, boxTransform);
	t[2] = GetTransformedPos(0.0f, 1.0f, boxTransform);
	t[3] = GetTransformedPos(1.0f, 1.0f, boxTransform);
	add_bounds();

	UNUSED_PARAMETER(scene);
	return true;
}

struct OffsetData {
	float clampDist;
	vec3 tl, br, offset;
};

static bool GetSourceSnapOffset(obs_scene_t *scene, obs_sceneitem_t *item,
				void *param)
{
	OffsetData *data = reinterpret_cast<OffsetData *>(param);

	if (obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 t[4] = {GetTransformedPos(0.0f, 0.0f, boxTransform),
		     GetTransformedPos(1.0f, 0.0f, boxTransform),
		     GetTransformedPos(0.0f, 1.0f, boxTransform),
		     GetTransformedPos(1.0f, 1.0f, boxTransform)};

	bool first = true;
	vec3 tl, br;
	vec3_zero(&tl);
	vec3_zero(&br);
	for (const vec3 &v : t) {
		if (first) {
			vec3_copy(&tl, &v);
			vec3_copy(&br, &v);
			first = false;
		} else {
			vec3_min(&tl, &tl, &v);
			vec3_max(&br, &br, &v);
		}
	}

	// Snap to other source edges
#define EDGE_SNAP(l, r, x, y)                                               \
	do {                                                                \
		double dist = fabsf(l.x - data->r.x);                       \
		if (dist < data->clampDist &&                               \
		    fabsf(data->offset.x) < EPSILON && data->tl.y < br.y && \
		    data->br.y > tl.y &&                                    \
		    (fabsf(data->offset.x) > dist ||                        \
		     data->offset.x < EPSILON))                             \
			data->offset.x = l.x - data->r.x;                   \
	} while (false)

	EDGE_SNAP(tl, br, x, y);
	EDGE_SNAP(tl, br, y, x);
	EDGE_SNAP(br, tl, x, y);
	EDGE_SNAP(br, tl, y, x);
#undef EDGE_SNAP

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::SnapItemMovement(vec2 &offset)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	SelectedItemBounds data;
	obs_scene_enum_items(scene, AddItemBounds, &data);

	data.tl.x += offset.x;
	data.tl.y += offset.y;
	data.br.x += offset.x;
	data.br.y += offset.y;

	vec3 snapOffset = GetSnapOffset(data.tl, data.br);

	const bool snap = config_get_bool(GetGlobalConfig(), "BasicWindow",
					  "SnappingEnabled");
	const bool sourcesSnap = config_get_bool(
		GetGlobalConfig(), "BasicWindow", "SourceSnapping");
	if (snap == false)
		return;
	if (sourcesSnap == false) {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
		return;
	}

	const float clampDist = config_get_double(GetGlobalConfig(),
						  "BasicWindow",
						  "SnapDistance") /
				main->previewScale;

	OffsetData offsetData;
	offsetData.clampDist = clampDist;
	offsetData.tl = data.tl;
	offsetData.br = data.br;
	vec3_copy(&offsetData.offset, &snapOffset);

	obs_scene_enum_items(scene, GetSourceSnapOffset, &offsetData);

	if (fabsf(offsetData.offset.x) > EPSILON ||
	    fabsf(offsetData.offset.y) > EPSILON) {
		offset.x += offsetData.offset.x;
		offset.y += offsetData.offset.y;
	} else {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
	}
}

static bool move_items(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	bool selected = obs_sceneitem_selected(item);
	vec2 *offset = reinterpret_cast<vec2 *>(param);

	if (obs_sceneitem_is_group(item) && !selected) {
		matrix4 transform;
		vec3 new_offset;
		vec3_set(&new_offset, offset->x, offset->y, 0.0f);

		obs_sceneitem_get_draw_transform(item, &transform);
		vec4_set(&transform.t, 0.0f, 0.0f, 0.0f, 1.0f);
		matrix4_inv(&transform, &transform);
		vec3_transform(&new_offset, &new_offset, &transform);
		obs_sceneitem_group_enum_items(item, move_items, &new_offset);
	}

	if (selected) {
		vec2 pos;
		obs_sceneitem_get_pos(item, &pos);
		vec2_add(&pos, &pos, offset);
		obs_sceneitem_set_pos(item, &pos);
	}

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::MoveItems(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();

	vec2 offset, moveOffset;
	vec2_sub(&offset, &pos, &startPos);
	vec2_sub(&moveOffset, &offset, &lastMoveOffset);

	if (!(modifiers & Qt::ControlModifier))
		SnapItemMovement(moveOffset);

	vec2_add(&lastMoveOffset, &lastMoveOffset, &moveOffset);

	obs_scene_enum_items(scene, move_items, &moveOffset);
}

static bool CounterClockwise(float x1, float x2, float x3, float y1, float y2,
			     float y3)
{
	return (y3 - y1) * (x2 - x1) > (y2 - y1) * (x3 - x1);
}

static bool IntersectLine(float x1, float x2, float x3, float x4, float y1,
			  float y2, float y3, float y4)
{
	bool a = CounterClockwise(x1, x2, x3, y1, y2, y3);
	bool b = CounterClockwise(x1, x2, x4, y1, y2, y4);
	bool c = CounterClockwise(x3, x4, x1, y3, y4, y1);
	bool d = CounterClockwise(x3, x4, x2, y3, y4, y2);

	return (a != b) && (c != d);
}

static bool IntersectBox(matrix4 transform, float x1, float x2, float y1,
			 float y2)
{
	float x3, x4, y3, y4;

	x3 = transform.t.x;
	y3 = transform.t.y;
	x4 = x3 + transform.x.x;
	y4 = y3 + transform.x.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x4 = x3 + transform.y.x;
	y4 = y3 + transform.y.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x3 = transform.t.x + transform.x.x;
	y3 = transform.t.y + transform.x.y;
	x4 = x3 + transform.y.x;
	y4 = y3 + transform.y.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x3 = transform.t.x + transform.y.x;
	y3 = transform.t.y + transform.y.y;
	x4 = x3 + transform.x.x;
	y4 = y3 + transform.x.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) ||
	    IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	return false;
}
#undef PI

bool OBSBasicPreview::FindSelected(obs_scene_t *scene, obs_sceneitem_t *item,
				   void *param)
{
	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);

	if (obs_sceneitem_selected(item))
		data->sceneItems.push_back(item);

	UNUSED_PARAMETER(scene);
	return true;
}

static bool FindItemsInBox(obs_scene_t *scene, obs_sceneitem_t *item,
			   void *param)
{
	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);
	matrix4 transform;
	matrix4 invTransform;
	vec3 transformedPos;
	vec3 pos3;
	vec3 pos3_;

	float x1 = std::min(data->startPos.x, data->pos.x);
	float x2 = std::max(data->startPos.x, data->pos.x);
	float y1 = std::min(data->startPos.y, data->pos.y);
	float y2 = std::max(data->startPos.y, data->pos.y);

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;
	if (!obs_sceneitem_visible(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	    transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x > x1 && transform.t.x < x2 && transform.t.y > y1 &&
	    transform.t.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.x.x > x1 &&
	    transform.t.x + transform.x.x < x2 &&
	    transform.t.y + transform.x.y > y1 &&
	    transform.t.y + transform.x.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.y.x > x1 &&
	    transform.t.x + transform.y.x < x2 &&
	    transform.t.y + transform.y.y > y1 &&
	    transform.t.y + transform.y.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.x.x + transform.y.x > x1 &&
	    transform.t.x + transform.x.x + transform.y.x < x2 &&
	    transform.t.y + transform.x.y + transform.y.y > y1 &&
	    transform.t.y + transform.x.y + transform.y.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + 0.5 * (transform.x.x + transform.y.x) > x1 &&
	    transform.t.x + 0.5 * (transform.x.x + transform.y.x) < x2 &&
	    transform.t.y + 0.5 * (transform.x.y + transform.y.y) > y1 &&
	    transform.t.y + 0.5 * (transform.x.y + transform.y.y) < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (IntersectBox(transform, x1, x2, y1, y2)) {
		data->sceneItems.push_back(item);
		return true;
	}

	UNUSED_PARAMETER(scene);
	return true;
}

void OBSBasicPreview::BoxItems(const vec2 &startPos, const vec2 &pos)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	if (cursor().shape() != Qt::CrossCursor)
		setCursor(Qt::CrossCursor);

	SceneFindBoxData data(startPos, pos);
	obs_scene_enum_items(scene, FindItemsInBox, &data);

	std::lock_guard<std::mutex> lock(selectMutex);
	hoveredPreviewItems = data.sceneItems;
}

vec3 OBSBasicPreview::CalculateStretchPos(const vec3 &tl, const vec3 &br)
{
	uint32_t alignment = obs_sceneitem_get_alignment(stretchItem);
	vec3 pos;

	vec3_zero(&pos);

	if (alignment & OBS_ALIGN_LEFT)
		pos.x = tl.x;
	else if (alignment & OBS_ALIGN_RIGHT)
		pos.x = br.x;
	else
		pos.x = (br.x - tl.x) * 0.5f + tl.x;

	if (alignment & OBS_ALIGN_TOP)
		pos.y = tl.y;
	else if (alignment & OBS_ALIGN_BOTTOM)
		pos.y = br.y;
	else
		pos.y = (br.y - tl.y) * 0.5f + tl.y;

	return pos;
}

void OBSBasicPreview::ClampAspect(vec3 &tl, vec3 &br, vec2 &size,
				  const vec2 &baseSize)
{
	float baseAspect = baseSize.x / baseSize.y;
	float aspect = size.x / size.y;
	uint32_t stretchFlags = (uint32_t)stretchHandle;

	if (stretchHandle == ItemHandle::TopLeft ||
	    stretchHandle == ItemHandle::TopRight ||
	    stretchHandle == ItemHandle::BottomLeft ||
	    stretchHandle == ItemHandle::BottomRight) {
		if (aspect < baseAspect) {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.x = size.y * baseAspect;
			else
				size.x = size.y * baseAspect * -1.0f;
		} else {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.y = size.x / baseAspect;
			else
				size.y = size.x / baseAspect * -1.0f;
		}

	} else if (stretchHandle == ItemHandle::TopCenter ||
		   stretchHandle == ItemHandle::BottomCenter) {
		if ((size.y >= 0.0f && size.x >= 0.0f) ||
		    (size.y <= 0.0f && size.x <= 0.0f))
			size.x = size.y * baseAspect;
		else
			size.x = size.y * baseAspect * -1.0f;

	} else if (stretchHandle == ItemHandle::CenterLeft ||
		   stretchHandle == ItemHandle::CenterRight) {
		if ((size.y >= 0.0f && size.x >= 0.0f) ||
		    (size.y <= 0.0f && size.x <= 0.0f))
			size.y = size.x / baseAspect;
		else
			size.y = size.x / baseAspect * -1.0f;
	}

	size.x = std::round(size.x);
	size.y = std::round(size.y);

	if (stretchFlags & ITEM_LEFT)
		tl.x = br.x - size.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = tl.x + size.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = br.y - size.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = tl.y + size.y;
}

void OBSBasicPreview::SnapStretchingToScreen(vec3 &tl, vec3 &br)
{
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	vec3 newTL = GetTransformedPos(tl.x, tl.y, itemToScreen);
	vec3 newTR = GetTransformedPos(br.x, tl.y, itemToScreen);
	vec3 newBL = GetTransformedPos(tl.x, br.y, itemToScreen);
	vec3 newBR = GetTransformedPos(br.x, br.y, itemToScreen);
	vec3 boundingTL;
	vec3 boundingBR;

	vec3_copy(&boundingTL, &newTL);
	vec3_min(&boundingTL, &boundingTL, &newTR);
	vec3_min(&boundingTL, &boundingTL, &newBL);
	vec3_min(&boundingTL, &boundingTL, &newBR);

	vec3_copy(&boundingBR, &newTL);
	vec3_max(&boundingBR, &boundingBR, &newTR);
	vec3_max(&boundingBR, &boundingBR, &newBL);
	vec3_max(&boundingBR, &boundingBR, &newBR);

	vec3 offset = GetSnapOffset(boundingTL, boundingBR);
	vec3_add(&offset, &offset, &newTL);
	vec3_transform(&offset, &offset, &screenToItem);
	vec3_sub(&offset, &offset, &tl);

	if (stretchFlags & ITEM_LEFT)
		tl.x += offset.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x += offset.x;

	if (stretchFlags & ITEM_TOP)
		tl.y += offset.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y += offset.y;
}

static float maxfunc(float x, float y)
{
	return x > y ? x : y;
}

static float minfunc(float x, float y)
{
	return x < y ? x : y;
}

void OBSBasicPreview::CropItem(const vec2 &pos)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	uint32_t align = obs_sceneitem_get_alignment(stretchItem);
	vec3 tl, br, pos3;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	obs_sceneitem_crop crop = startCrop;
	vec2 scale;

	obs_sceneitem_get_scale(stretchItem, &scale);

	vec2 max_tl;
	vec2 max_br;

	vec2_set(&max_tl, float(-crop.left) * scale.x,
		 float(-crop.top) * scale.y);
	vec2_set(&max_br, stretchItemSize.x + crop.right * scale.x,
		 stretchItemSize.y + crop.bottom * scale.y);

	typedef std::function<float(float, float)> minmax_func_t;

	minmax_func_t min_x = scale.x < 0.0f ? maxfunc : minfunc;
	minmax_func_t min_y = scale.y < 0.0f ? maxfunc : minfunc;
	minmax_func_t max_x = scale.x < 0.0f ? minfunc : maxfunc;
	minmax_func_t max_y = scale.y < 0.0f ? minfunc : maxfunc;

	pos3.x = min_x(pos3.x, max_br.x);
	pos3.x = max_x(pos3.x, max_tl.x);
	pos3.y = min_y(pos3.y, max_br.y);
	pos3.y = max_y(pos3.y, max_tl.y);

	if (stretchFlags & ITEM_LEFT) {
		float maxX = stretchItemSize.x - (2.0 * scale.x);
		pos3.x = tl.x = min_x(pos3.x, maxX);

	} else if (stretchFlags & ITEM_RIGHT) {
		float minX = (2.0 * scale.x);
		pos3.x = br.x = max_x(pos3.x, minX);
	}

	if (stretchFlags & ITEM_TOP) {
		float maxY = stretchItemSize.y - (2.0 * scale.y);
		pos3.y = tl.y = min_y(pos3.y, maxY);

	} else if (stretchFlags & ITEM_BOTTOM) {
		float minY = (2.0 * scale.y);
		pos3.y = br.y = max_y(pos3.y, minY);
	}

#define ALIGN_X (ITEM_LEFT | ITEM_RIGHT)
#define ALIGN_Y (ITEM_TOP | ITEM_BOTTOM)
	vec3 newPos;
	vec3_zero(&newPos);

	uint32_t align_x = (align & ALIGN_X);
	uint32_t align_y = (align & ALIGN_Y);
	if (align_x == (stretchFlags & ALIGN_X) && align_x != 0)
		newPos.x = pos3.x;
	else if (align & ITEM_RIGHT)
		newPos.x = stretchItemSize.x;
	else if (!(align & ITEM_LEFT))
		newPos.x = stretchItemSize.x * 0.5f;

	if (align_y == (stretchFlags & ALIGN_Y) && align_y != 0)
		newPos.y = pos3.y;
	else if (align & ITEM_BOTTOM)
		newPos.y = stretchItemSize.y;
	else if (!(align & ITEM_TOP))
		newPos.y = stretchItemSize.y * 0.5f;
#undef ALIGN_X
#undef ALIGN_Y

	crop = startCrop;

	if (stretchFlags & ITEM_LEFT)
		crop.left += int(std::round(tl.x / scale.x));
	else if (stretchFlags & ITEM_RIGHT)
		crop.right +=
			int(std::round((stretchItemSize.x - br.x) / scale.x));

	if (stretchFlags & ITEM_TOP)
		crop.top += int(std::round(tl.y / scale.y));
	else if (stretchFlags & ITEM_BOTTOM)
		crop.bottom +=
			int(std::round((stretchItemSize.y - br.y) / scale.y));

	vec3_transform(&newPos, &newPos, &itemToScreen);
	newPos.x = std::round(newPos.x);
	newPos.y = std::round(newPos.y);

#if 0
	vec3 curPos;
	vec3_zero(&curPos);
	obs_sceneitem_get_pos(stretchItem, (vec2*)&curPos);
	blog(LOG_DEBUG, "curPos {%d, %d} - newPos {%d, %d}",
			int(curPos.x), int(curPos.y),
			int(newPos.x), int(newPos.y));
	blog(LOG_DEBUG, "crop {%d, %d, %d, %d}",
			crop.left, crop.top,
			crop.right, crop.bottom);
#endif

	obs_sceneitem_defer_update_begin(stretchItem);
	obs_sceneitem_set_crop(stretchItem, &crop);
	if (boundsType == OBS_BOUNDS_NONE)
		obs_sceneitem_set_pos(stretchItem, (vec2 *)&newPos);
	obs_sceneitem_defer_update_end(stretchItem);
}

void OBSBasicPreview::StretchItem(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	vec3 tl, br, pos3;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	if (stretchFlags & ITEM_LEFT)
		tl.x = pos3.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = pos3.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = pos3.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = pos3.y;

	if (!(modifiers & Qt::ControlModifier))
		SnapStretchingToScreen(tl, br);

	obs_source_t *source = obs_sceneitem_get_source(stretchItem);

	vec2 baseSize;
	vec2_set(&baseSize, float(obs_source_get_width(source)),
		 float(obs_source_get_height(source)));

	vec2 size;
	vec2_set(&size, br.x - tl.x, br.y - tl.y);

	if (boundsType != OBS_BOUNDS_NONE) {
		if (shiftDown)
			ClampAspect(tl, br, size, baseSize);

		if (tl.x > br.x)
			std::swap(tl.x, br.x);
		if (tl.y > br.y)
			std::swap(tl.y, br.y);

		vec2_abs(&size, &size);

		obs_sceneitem_set_bounds(stretchItem, &size);
	} else {
		obs_sceneitem_crop crop;
		obs_sceneitem_get_crop(stretchItem, &crop);

		baseSize.x -= float(crop.left + crop.right);
		baseSize.y -= float(crop.top + crop.bottom);

		if (!shiftDown)
			ClampAspect(tl, br, size, baseSize);

		vec2_div(&size, &size, &baseSize);
		obs_sceneitem_set_scale(stretchItem, &size);
	}

	pos3 = CalculateStretchPos(tl, br);
	vec3_transform(&pos3, &pos3, &itemToScreen);

	vec2 newPos;
	vec2_set(&newPos, std::round(pos3.x), std::round(pos3.y));
	obs_sceneitem_set_pos(stretchItem, &newPos);
}

void OBSBasicPreview::mouseMoveEvent(QMouseEvent *event)
{
	if (scrollMode && event->buttons() == Qt::LeftButton) {
		scrollingOffset.x += event->x() - scrollingFrom.x;
		scrollingOffset.y += event->y() - scrollingFrom.y;
		scrollingFrom.x = event->x();
		scrollingFrom.y = event->y();
		emit DisplayResized();
		return;
	}

	if (locked)
		return;

	bool updateCursor = false;

	if (mouseDown) {
		vec2 pos = GetMouseEventPos(event);

		if (!mouseMoved && !mouseOverItems &&
		    stretchHandle == ItemHandle::None) {
			ProcessClick(startPos);
			mouseOverItems = SelectedAtPos(startPos);
		}

		pos.x = std::round(pos.x);
		pos.y = std::round(pos.y);

		if (stretchHandle != ItemHandle::None) {
			selectionBox = false;

			OBSBasic *main = reinterpret_cast<OBSBasic *>(
				App()->GetMainWindow());
			OBSScene scene = main->GetCurrentScene();
			obs_sceneitem_t *group =
				obs_sceneitem_get_group(scene, stretchItem);
			if (group) {
				vec3 group_pos;
				vec3_set(&group_pos, pos.x, pos.y, 0.0f);
				vec3_transform(&group_pos, &group_pos,
					       &invGroupTransform);
				pos.x = group_pos.x;
				pos.y = group_pos.y;
			}

			if (cropping)
				CropItem(pos);
			else
				StretchItem(pos);

		} else if (mouseOverItems) {
			if (cursor().shape() != Qt::SizeAllCursor)
				setCursor(Qt::SizeAllCursor);
			selectionBox = false;
			MoveItems(pos);
		} else {
			selectionBox = true;
			if (!mouseMoved)
				DoSelect(startPos);
			BoxItems(startPos, pos);
		}

		mouseMoved = true;
		mousePos = pos;
	} else {
		vec2 pos = GetMouseEventPos(event);
		OBSSceneItem item = GetItemAtPos(pos, true);

		std::lock_guard<std::mutex> lock(selectMutex);
		hoveredPreviewItems.clear();
		hoveredPreviewItems.push_back(item);

		if (!mouseMoved && hoveredPreviewItems.size() > 0) {
			mousePos = pos;
			OBSBasic *main = reinterpret_cast<OBSBasic *>(
				App()->GetMainWindow());
#ifdef SUPPORTS_FRACTIONAL_SCALING
			float scale = main->devicePixelRatioF();
#else
			float scale = main->devicePixelRatio();
#endif
			float x = float(event->x()) - main->previewX / scale;
			float y = float(event->y()) - main->previewY / scale;
			vec2_set(&startPos, x, y);
			updateCursor = true;
		}
	}

	if (updateCursor) {
		GetStretchHandleData(startPos);
		uint32_t stretchFlags = (uint32_t)stretchHandle;
		UpdateCursor(stretchFlags);
	}
}

void OBSBasicPreview::leaveEvent(QEvent *)
{
	std::lock_guard<std::mutex> lock(selectMutex);
	if (!selectionBox)
		hoveredPreviewItems.clear();
}

static void DrawSquareAtPos(float x, float y)
{
	struct vec3 pos;
	vec3_set(&pos, x, y, 0.0f);

	struct matrix4 matrix;
	gs_matrix_get(&matrix);
	vec3_transform(&pos, &pos, &matrix);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate(&pos);

	gs_matrix_translate3f(-HANDLE_RADIUS, -HANDLE_RADIUS, 0.0f);
	gs_matrix_scale3f(HANDLE_RADIUS * 2, HANDLE_RADIUS * 2, 1.0f);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();
}

static void DrawLine(float x1, float y1, float x2, float y2, float thickness,
		     vec2 scale)
{
	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	gs_render_start(true);

	gs_vertex2f(x1, y1);
	gs_vertex2f(x1 + (xSide * (thickness / scale.x)),
		    y1 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2, y2);
	gs_vertex2f(x2 + (xSide * (thickness / scale.x)),
		    y2 + (ySide * (thickness / scale.y)));

	gs_vertbuffer_t *line = gs_render_save();

	gs_load_vertexbuffer(line);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(line);
}

static void DrawRect(float thickness, vec2 scale)
{
	gs_render_start(true);

	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f + (thickness / scale.x), 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(0.0f + (thickness / scale.x), 1.0f);
	gs_vertex2f(0.0f, 1.0f - (thickness / scale.y));
	gs_vertex2f(1.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f - (thickness / scale.y));
	gs_vertex2f(1.0f - (thickness / scale.x), 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f - (thickness / scale.x), 0.0f);
	gs_vertex2f(1.0f, 0.0f + (thickness / scale.y));
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 0.0f + (thickness / scale.y));

	gs_vertbuffer_t *rect = gs_render_save();

	gs_load_vertexbuffer(rect);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(rect);
}

static inline bool crop_enabled(const obs_sceneitem_crop *crop)
{
	return crop->left > 0 || crop->top > 0 || crop->right > 0 ||
	       crop->bottom > 0;
}

bool OBSBasicPreview::DrawSelectedOverflow(obs_scene_t *scene,
					   obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	if (!SceneItemHasVideo(item))
		return true;

	bool select = config_get_bool(GetGlobalConfig(), "BasicWindow",
				      "OverflowSelectionHidden");

	if (!select && !obs_sceneitem_visible(item))
		return true;

	if (obs_sceneitem_is_group(item)) {
		matrix4 mat;
		obs_sceneitem_get_draw_transform(item, &mat);

		gs_matrix_push();
		gs_matrix_mul(&mat);
		obs_sceneitem_group_enum_items(item, DrawSelectedOverflow,
					       param);
		gs_matrix_pop();
	}

	bool always = config_get_bool(GetGlobalConfig(), "BasicWindow",
				      "OverflowAlwaysVisible");

	if (!always && !obs_sceneitem_selected(item))
		return true;

	OBSBasicPreview *prev = reinterpret_cast<OBSBasicPreview *>(param);

	matrix4 boxTransform;
	matrix4 invBoxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);
	matrix4_inv(&invBoxTransform, &boxTransform);

	vec3 bounds[] = {
		{{{0.f, 0.f, 0.f}}},
		{{{1.f, 0.f, 0.f}}},
		{{{0.f, 1.f, 0.f}}},
		{{{1.f, 1.f, 0.f}}},
	};

	bool visible = std::all_of(
		std::begin(bounds), std::end(bounds), [&](const vec3 &b) {
			vec3 pos;
			vec3_transform(&pos, &b, &boxTransform);
			vec3_transform(&pos, &pos, &invBoxTransform);
			return CloseFloat(pos.x, b.x) && CloseFloat(pos.y, b.y);
		});

	if (!visible)
		return true;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawSelectedOverflow");

	obs_transform_info info;
	obs_sceneitem_get_info(item, &info);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_REPEAT);
	gs_eparam_t *image = gs_effect_get_param_by_name(solid, "image");
	gs_eparam_t *scale = gs_effect_get_param_by_name(solid, "scale");

	vec2 s;
	vec2_set(&s, boxTransform.x.x / 96, boxTransform.y.y / 96);

	gs_effect_set_vec2(scale, &s);
	gs_effect_set_texture(image, prev->overflow);

	gs_matrix_push();
	gs_matrix_mul(&boxTransform);

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	while (gs_effect_loop(solid, "Draw")) {
		gs_draw_sprite(prev->overflow, 0, 1, 1);
	}

	gs_matrix_pop();

	GS_DEBUG_MARKER_END();

	UNUSED_PARAMETER(scene);
	return true;
}

bool OBSBasicPreview::DrawSelectedItem(obs_scene_t *scene,
				       obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	if (!SceneItemHasVideo(item))
		return true;

	if (obs_sceneitem_is_group(item)) {
		matrix4 mat;
		obs_sceneitem_get_draw_transform(item, &mat);

		gs_matrix_push();
		gs_matrix_mul(&mat);
		obs_sceneitem_group_enum_items(item, DrawSelectedItem, param);
		gs_matrix_pop();
	}

	OBSBasicPreview *prev = reinterpret_cast<OBSBasicPreview *>(param);
	OBSBasic *main = OBSBasic::Get();

	bool hovered = false;
	{
		std::lock_guard<std::mutex> lock(prev->selectMutex);
		for (size_t i = 0; i < prev->hoveredPreviewItems.size(); i++) {
			if (prev->hoveredPreviewItems[i] == item) {
				hovered = true;
				break;
			}
		}
	}

	bool selected = obs_sceneitem_selected(item);

	if (!selected && !hovered)
		return true;

	matrix4 boxTransform;
	matrix4 invBoxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);
	matrix4_inv(&invBoxTransform, &boxTransform);

	vec3 bounds[] = {
		{{{0.f, 0.f, 0.f}}},
		{{{1.f, 0.f, 0.f}}},
		{{{0.f, 1.f, 0.f}}},
		{{{1.f, 1.f, 0.f}}},
	};

	vec4 red;
	vec4 green;
	vec4 blue;

	vec4_set(&red, 1.0f, 0.0f, 0.0f, 1.0f);
	vec4_set(&green, 0.0f, 1.0f, 0.0f, 1.0f);
	vec4_set(&blue, 0.0f, 0.5f, 1.0f, 1.0f);

	bool visible = std::all_of(
		std::begin(bounds), std::end(bounds), [&](const vec3 &b) {
			vec3 pos;
			vec3_transform(&pos, &b, &boxTransform);
			vec3_transform(&pos, &pos, &invBoxTransform);
			return CloseFloat(pos.x, b.x) && CloseFloat(pos.y, b.y);
		});

	if (!visible)
		return true;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawSelectedItem");

	matrix4 curTransform;
	vec2 boxScale;
	gs_matrix_get(&curTransform);
	obs_sceneitem_get_box_scale(item, &boxScale);
	boxScale.x *= curTransform.x.x;
	boxScale.y *= curTransform.y.y;

	obs_transform_info info;
	obs_sceneitem_get_info(item, &info);

	gs_matrix_push();
	gs_matrix_mul(&boxTransform);

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	gs_effect_t *eff = gs_get_effect();
	gs_eparam_t *colParam = gs_effect_get_param_by_name(eff, "color");

	if (info.bounds_type == OBS_BOUNDS_NONE && crop_enabled(&crop)) {
#define DRAW_SIDE(side, x1, y1, x2, y2)                        \
	if (hovered && !selected)                              \
		gs_effect_set_vec4(colParam, &blue);           \
	else if (crop.side > 0)                                \
		gs_effect_set_vec4(colParam, &green);          \
	DrawLine(x1, y1, x2, y2, HANDLE_RADIUS / 2, boxScale); \
	gs_effect_set_vec4(colParam, &red);

		DRAW_SIDE(left, 0.0f, 0.0f, 0.0f, 1.0f);
		DRAW_SIDE(top, 0.0f, 0.0f, 1.0f, 0.0f);
		DRAW_SIDE(right, 1.0f, 0.0f, 1.0f, 1.0f);
		DRAW_SIDE(bottom, 0.0f, 1.0f, 1.0f, 1.0f);
#undef DRAW_SIDE
	} else {
		if (!selected) {
			gs_effect_set_vec4(colParam, &blue);
			DrawRect(HANDLE_RADIUS / 2, boxScale);
		} else {
			DrawRect(HANDLE_RADIUS / 2, boxScale);
		}
	}

	gs_load_vertexbuffer(main->box);
	gs_effect_set_vec4(colParam, &red);

	if (selected) {
		DrawSquareAtPos(0.0f, 0.0f);
		DrawSquareAtPos(0.0f, 1.0f);
		DrawSquareAtPos(1.0f, 0.0f);
		DrawSquareAtPos(1.0f, 1.0f);
		DrawSquareAtPos(0.5f, 0.0f);
		DrawSquareAtPos(0.0f, 0.5f);
		DrawSquareAtPos(0.5f, 1.0f);
		DrawSquareAtPos(1.0f, 0.5f);
	}

	gs_matrix_pop();

	GS_DEBUG_MARKER_END();

	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);
	return true;
}

bool OBSBasicPreview::DrawSelectionBox(float x1, float y1, float x2, float y2,
				       gs_vertbuffer_t *rectFill)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *eff = gs_get_effect();
	gs_eparam_t *colParam = gs_effect_get_param_by_name(eff, "color");

	vec4 fillColor;
	vec4_set(&fillColor, 0.7f, 0.7f, 0.7f, 0.5f);

	vec4 borderColor;
	vec4_set(&borderColor, 1.0f, 1.0f, 1.0f, 1.0f);

	vec2 scale;
	vec2_set(&scale, std::abs(x2 - x1), std::abs(y2 - y1));

	gs_matrix_push();
	gs_matrix_identity();

	gs_matrix_translate3f(x1, y1, 0.0f);
	gs_matrix_scale3f(x2 - x1, y2 - y1, 1.0f);

	gs_effect_set_vec4(colParam, &fillColor);
	gs_load_vertexbuffer(rectFill);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_effect_set_vec4(colParam, &borderColor);
	DrawRect(HANDLE_RADIUS / 2, scale);

	gs_matrix_pop();

	return true;
}

void OBSBasicPreview::DrawOverflow()
{
	if (locked)
		return;

	bool hidden = config_get_bool(GetGlobalConfig(), "BasicWindow",
				      "OverflowHidden");

	if (hidden)
		return;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawOverflow");

	if (!overflow) {
		std::string path;
		GetDataFilePath("images/overflow.png", path);
		overflow = gs_texture_create_from_file(path.c_str());
	}

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	OBSScene scene = main->GetCurrentScene();

	if (scene) {
		gs_matrix_push();
		gs_matrix_scale3f(main->previewScale, main->previewScale, 1.0f);
		obs_scene_enum_items(scene, DrawSelectedOverflow, this);
		gs_matrix_pop();
	}

	gs_load_vertexbuffer(nullptr);

	GS_DEBUG_MARKER_END();
}

void OBSBasicPreview::DrawSceneEditing()
{
	if (locked)
		return;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawSceneEditing");

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 color;
	vec4_set(&color, 1.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	OBSScene scene = main->GetCurrentScene();

	if (scene) {
		gs_matrix_push();
		gs_matrix_scale3f(main->previewScale, main->previewScale, 1.0f);
		obs_scene_enum_items(scene, DrawSelectedItem, this);
		gs_matrix_pop();
	}

	if (selectionBox) {
		if (!rectFill) {
			gs_render_start(true);

			gs_vertex2f(0.0f, 0.0f);
			gs_vertex2f(1.0f, 0.0f);
			gs_vertex2f(0.0f, 1.0f);
			gs_vertex2f(1.0f, 1.0f);

			rectFill = gs_render_save();
		}

		DrawSelectionBox(startPos.x * main->previewScale,
				 startPos.y * main->previewScale,
				 mousePos.x * main->previewScale,
				 mousePos.y * main->previewScale, rectFill);
	}

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	GS_DEBUG_MARKER_END();
}

void OBSBasicPreview::ResetScrollingOffset()
{
	vec2_zero(&scrollingOffset);
}

void OBSBasicPreview::SetScalingLevel(int32_t newScalingLevelVal)
{
	float newScalingAmountVal =
		pow(ZOOM_SENSITIVITY, float(newScalingLevelVal));
	scalingLevel = newScalingLevelVal;
	SetScalingAmount(newScalingAmountVal);
}

void OBSBasicPreview::SetScalingAmount(float newScalingAmountVal)
{
	scrollingOffset.x *= newScalingAmountVal / scalingAmount;
	scrollingOffset.y *= newScalingAmountVal / scalingAmount;
	scalingAmount = newScalingAmountVal;
}

OBSBasicPreview *OBSBasicPreview::Get()
{
	return OBSBasic::Get()->ui->preview;
}
