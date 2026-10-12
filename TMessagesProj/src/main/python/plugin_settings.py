from finegram import runtime

def init():
    pass

def get_setting(plugin_id, key, default=None):
    return runtime.get_setting(plugin_id, key, default)

def set_setting(plugin_id, key, value):
    runtime.set_setting(plugin_id, key, value)

def get_all_settings(plugin_id):
    return runtime.export_settings(plugin_id)

def set_all_settings(plugin_id, values):
    runtime.import_settings(plugin_id, values)

def clear_settings(plugin_id):
    stored = runtime.export_settings(plugin_id)
    if stored:
        runtime.import_settings(plugin_id, {key: None for key in stored})
