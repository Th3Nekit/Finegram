class FinegramPlugin:

    id = ""
    name = ""
    version = ""
    author = ""
    description = ""

    def on_load(self):
        pass

    def on_unload(self):
        pass

    def on_setting_changed(self, key, value):
        pass

    def log(self, message):
        from finegram import runtime
        runtime.log(f"[{self.id}] {message}")

    def get_setting(self, key, default=None):
        from finegram import runtime
        return runtime.get_setting(self.id, key, default)

    def set_setting(self, key, value):
        from finegram import runtime
        runtime.set_setting(self.id, key, value)
