obs = obslua
pause_scene = ""

function on_event(event)
	if event == obs.OBS_FRONTEND_EVENT_SCENE_CHANGED then
		local scene = obs.obs_frontend_get_current_scene()
		local scene_name = obs.obs_source_get_name(scene)
		if pause_scene == scene_name then
			obs.obs_frontend_recording_pause(true)
		else
			obs.obs_frontend_recording_pause(false)
		end

		obs.obs_source_release(scene);
	end
end

function script_properties()
	local props = obs.obs_properties_create()

	local p = obs.obs_properties_add_list(props, "pause_scene", "Pause Scene", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
	local scenes = obs.obs_frontend_get_scenes()
	if scenes ~= nil then
		for _, scene in ipairs(scenes) do
			local name = obs.obs_source_get_name(scene);
			obs.obs_property_list_add_string(p, name, name)
		end
	end
	obs.source_list_release(scenes)

	return props
end

function script_description()
	return "Adds the ability to pause recording when switching to a specific scene"
end

function script_update(settings)
	pause_scene = obs.obs_data_get_string(settings, "pause_scene")
end

function script_load(settings)
	obs.obs_frontend_add_event_callback(on_event)
end
