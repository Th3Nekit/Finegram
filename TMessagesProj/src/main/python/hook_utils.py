from finegram import runtime

try:
    from java import jclass
except ImportError:
    jclass = None

def find_class(class_name):

    return runtime.find_class(class_name)

def get_private_field(obj, field_name):

    return runtime.get_private_field(obj, field_name)

def set_private_field(obj, field_name, value):

    runtime.set_private_field(obj, field_name, value)

def get_private_field_of_class(class_name, field_name):

    target = runtime.find_class(class_name)
    return None if target is None else runtime.get_private_field(target, field_name)

def load_dex(data, key=None):

    return runtime.load_dex(data, key)

def load_dex_class(loader, class_name):

    return runtime.load_dex_class(loader, class_name)

def unpack_payload(path, begin_marker, end_marker):

    return runtime.unpack_payload(path, begin_marker, end_marker)

def get_static_private_field(class_name, field_name):

    target = class_name if not isinstance(class_name, str) else runtime.find_class(class_name)
    return None if target is None else runtime.get_private_field(target, field_name)

def set_static_private_field(class_name, field_name, value):

    target = class_name if not isinstance(class_name, str) else runtime.find_class(class_name)
    if target is None:
        return False
    runtime.set_private_field(target, field_name, value)
    return True

def log(message):

    runtime.log(message)

def get_field(obj, field_name):

    return get_private_field(obj, field_name)

def set_field(obj, field_name, value):
    set_private_field(obj, field_name, value)

def jarray(element_type):
    from java import jarray as _jarray

    return _jarray(element_type)
