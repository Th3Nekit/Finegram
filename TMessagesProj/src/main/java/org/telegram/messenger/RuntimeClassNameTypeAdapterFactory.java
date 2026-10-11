package org.telegram.messenger;

import com.google.gson.ExclusionStrategy;
import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonPrimitive;
import com.google.gson.TypeAdapter;
import com.google.gson.TypeAdapterFactory;
import com.google.gson.internal.Streams;
import com.google.gson.reflect.TypeToken;
import com.google.gson.stream.JsonReader;
import com.google.gson.stream.JsonWriter;

import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;

public final class RuntimeClassNameTypeAdapterFactory<T> implements TypeAdapterFactory {
    private final Class<?> baseType;
    private final String typeFieldName;
    private final Map<String, Class<?>> labelToSubtype = new LinkedHashMap<String, Class<?>>();
    private final Map<Class<?>, String> subtypeToLabel = new LinkedHashMap<Class<?>, String>();
    private final ExclusionStrategy exclusionStrategy;

    private RuntimeClassNameTypeAdapterFactory(Class<?> baseType, String typeFieldName, ExclusionStrategy exclusionStrategy) {
        if (typeFieldName == null || baseType == null) {
            throw new NullPointerException();
        }
        this.baseType = baseType;
        this.typeFieldName = typeFieldName;
        this.exclusionStrategy = exclusionStrategy;
    }

    public static <T> RuntimeClassNameTypeAdapterFactory<T> of(Class<T> baseType, String typeFieldName, ExclusionStrategy exclusionStrategy) {
        return new RuntimeClassNameTypeAdapterFactory<T>(baseType, typeFieldName, exclusionStrategy);
    }

    public static <T> RuntimeClassNameTypeAdapterFactory<T> of(Class<T> baseType) {
        return new RuntimeClassNameTypeAdapterFactory<T>(baseType, "class", null);
    }

    public RuntimeClassNameTypeAdapterFactory<T> registerSubtype(Class<? extends T> type, String label) {
        if (type == null || label == null) {
            throw new NullPointerException();
        }
        if (subtypeToLabel.containsKey(type) || labelToSubtype.containsKey(label)) {
            throw new IllegalArgumentException("types and labels must be unique");
        }
        labelToSubtype.put(label, type);
        subtypeToLabel.put(type, label);
        return this;
    }

    public RuntimeClassNameTypeAdapterFactory<T> registerSubtype(Class<? extends T> type) {
        return registerSubtype(type, type.getSimpleName());
    }

    public <R> TypeAdapter<R> create(Gson gson, TypeToken<R> type) {

        if (exclusionStrategy.shouldSkipClass(type.getRawType().getClass())) {
            return null;
        }
        final Map<String, TypeAdapter<?>> labelToDelegate
                = new LinkedHashMap<String, TypeAdapter<?>>();
        final Map<Class<?>, TypeAdapter<?>> subtypeToDelegate
                = new LinkedHashMap<Class<?>, TypeAdapter<?>>();

        if (Object.class.isAssignableFrom(type.getRawType())) {
            TypeAdapter<?> delegate = gson.getDelegateAdapter(this, type);
            labelToDelegate.put(type.getRawType().getSimpleName(), delegate);
            subtypeToDelegate.put(type.getRawType(), delegate);
        }

        return new TypeAdapter<R>() {
            @SuppressWarnings("unchecked")
            @Override
            public R read(JsonReader in) throws IOException {
                JsonElement jsonElement = Streams.parse(in);
                if (jsonElement.isJsonObject()) {
                    JsonElement labelJsonElement = jsonElement.getAsJsonObject().remove(typeFieldName);
                    if (labelJsonElement == null) {
                        throw new JsonParseException("cannot deserialize " + baseType
                                + " because it does not define a field named " + typeFieldName);
                    }
                    String label = labelJsonElement.getAsString();
                    TypeAdapter<R> delegate = (TypeAdapter<R>) labelToDelegate.get(label);
                    if (delegate == null) {
                        Class<R> aClass;
                        try {
                            aClass = (Class<R>) Class.forName(label);
                        } catch (ClassNotFoundException e) {
                            throw new JsonParseException("Cannot find class " + label, e);
                        }

                        TypeToken<R> subClass = TypeToken.get(aClass);
                        delegate = gson.getDelegateAdapter(RuntimeClassNameTypeAdapterFactory.this, subClass);
                        if (delegate == null) {
                            throw new JsonParseException("cannot deserialize " + baseType + " subtype named "
                                    + label + "; did you forget to register a subtype?");
                        }
                    }
                    return delegate.fromJsonTree(jsonElement);
                } else if (jsonElement.isJsonNull()) {
                    return null;
                } else {
                    TypeAdapter<R> delegate = gson.getDelegateAdapter(RuntimeClassNameTypeAdapterFactory.this, type);
                    if (delegate == null) {
                        throw new JsonParseException("cannot deserialize " + baseType + "; did you forget to register a subtype?");
                    }
                    return delegate.fromJsonTree(jsonElement);
                }
            }

            @Override
            public void write(JsonWriter out, R value) throws IOException {
                Class<?> srcType = value.getClass();
                String label = srcType.getSimpleName();
                TypeAdapter<R> delegate = getDelegate(srcType);
                if (delegate == null) {
                    throw new JsonParseException("cannot serialize " + srcType.getSimpleName()
                            + "; did you forget to register a subtype?");
                }
                JsonElement jsonTree = delegate.toJsonTree(value);
                if (!jsonTree.isJsonObject()) {
                    Streams.write(jsonTree, out);
                } else {
                    JsonObject jsonObject = jsonTree.getAsJsonObject();
                    if (jsonObject.has(typeFieldName)) {
                        throw new JsonParseException("cannot serialize " + srcType.getSimpleName()
                                + " because it already defines a field named " + typeFieldName);
                    }
                    JsonObject clone = new JsonObject();
                    clone.add(typeFieldName, new JsonPrimitive(label));
                    for (Map.Entry<String, JsonElement> e : jsonObject.entrySet()) {
                        clone.add(e.getKey(), e.getValue());
                    }
                    Streams.write(clone, out);
                }
            }

            @SuppressWarnings("unchecked")
            private TypeAdapter<R> getDelegate(Class<?> srcType) {
                TypeAdapter<?> typeAdapter = subtypeToDelegate.get(srcType);
                if (typeAdapter != null) {
                    return (TypeAdapter<R>) typeAdapter;
                }

                for (Map.Entry<Class<?>, TypeAdapter<?>> classTypeAdapterEntry : subtypeToDelegate.entrySet()) {
                    if (classTypeAdapterEntry.getKey().isAssignableFrom(srcType)) {
                        return (TypeAdapter<R>) classTypeAdapterEntry.getValue();
                    }
                }
                return null;
            }
        }.nullSafe();
    }
}