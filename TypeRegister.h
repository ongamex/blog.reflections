#pragma once

#include <assert.h>
#include <string.h>
#include <type_traits>
#include <vector>
#include <map>


// This is the way we generate id's form types. This could be replaced with anything: custom number, a thing based on the name of the class, ect.
// I guess the built in typeid is fine too!
typedef void (*QuickTypeIdType)();
template<typename T>
struct QuickTypeId
{
	static void value() {}
};

// The standard offsetof doesn't work with pointer to members under GCC.
template<typename T, typename M> 
constexpr int my_offsetof(M T::*member)
{
	return (int)((char*)&((T*)nullptr->*member) - (char*)nullptr);
}

//-----------------------------------------------------
// These are just some flags that are totally general puropuse. 
// The reflection system doesn't depend on these flags, you could totally delete them or replace them with other
// structures if you want.
//--------------------------------------------------------------------------
enum MemberFieldFlags : unsigned
{
	MFF_Editable = 1 << 0, // Set if t
	MFF_Saveable = 1 << 1,
};

// This structure is describes a data member in a struct/class.
struct MemberFieldDesc
{
	const char* name = nullptr; // A pointer ot a constant with the name of the variable.
	QuickTypeIdType quickTypeId = 0; // The type id of the variable.
	int byteOffset = 0; // The byte offset in the structure.
	int sizeBytes = 0; // This isn't really needed as we have the type of the member here, however this is commonly used so I cache it here for simplicity.
	QuickTypeIdType inheritedForm = 0; // If non-zero this, identifies that this variable was originally inherited form othet type. The reflection system doesn't depend on this.

	// Custom flags. see enum MemberFieldFlags. This isn't needed by the reflection system.
	unsigned int flags = 0; 
};

// A commonly used strcture. We do a fwddecl to potentially reduce the compile times.
extern template class std::vector<MemberFieldDesc>;

//--------------------------------------------------------------------------
// This is the structure that represes a type in the reflection system.
// All the member datas are stored and described here.
//--------------------------------------------------------------------------
struct TypeDesc
{
	const char* name = nullptr;
	QuickTypeIdType quickTypeId = nullptr;
	int sizeBytes = 0;
	
	QuickTypeIdType enumUnderlayingType = nullptr; // if set, the type that is used to represent an enum. If the type is not enum this is nullptr.
	QuickTypeIdType stdVectorUnderlayingType = nullptr; // if set, the type is std::vector. nullptr otherwise. The reflecton system doesn't rely on this to work.

	std::vector<MemberFieldDesc> members; // Contains all the members described by the type.
	std::vector<QuickTypeIdType> superclasses; // Who are we inheriting from basically, multiple inheritance isn't yet supported!.
	void (*copyFn)(void* dest, const void* src) = nullptr;
	bool (*equalsFn)(const void* a, const void* b) = nullptr;
	void (*constructorFn)(void* dest) = nullptr;
	void (*destructorFn)(void* dest) = nullptr;

	// A set of function used only if the current type is std::vector<?>.
	// The reflecton system doesn't rely on this to work.
	size_t (*stdVectorSize)(const void* vector) = nullptr;
	void (*stdVectorResize)(void* vector, size_t size) = nullptr;
	void* (*stdVectorGetElement)(void* vector, size_t index) = nullptr;
	const void* (*stdVectorGetElementConst)(const void* vector, size_t index) = nullptr;

	template<typename T>
	static TypeDesc create(const char* const name)
	{
		TypeDesc td;

		td.name = name;
		td.quickTypeId = QuickTypeId<T>::value;
		td.sizeBytes = sizeof(T);

		return td;
	}

	// Marks the type as default constrctable, and stores a functions that could call constrctor/desctructor. This could be detected automatically I guess.
	template<typename T>
	TypeDesc& constructable() {
		assert(QuickTypeId<T>::value == quickTypeId);
		
		constructorFn = [](void* dest) {
			new((T*)dest) T();
		};

		destructorFn = [](void* dest) {
			((T*)dest)->~T();
		};

		return *this;
	}

	// Marks the class as copyable and stores a function that can call that assign operator. This could be detected automatically I guess.
	template<typename T>
	TypeDesc& copyable() {
		assert(QuickTypeId<T>::value == quickTypeId);
		copyFn = [](void* dest, const void* src) {
			*(T*)(dest) = *(T*)(src);
		};

		return *this;
	}

	// Marks that the class is comaprable and stores a function that can call operator==. This could be detected automatically I guess.
	template<typename T>
	TypeDesc& compareable() {
		assert(QuickTypeId<T>::value == quickTypeId);
		equalsFn = [](const void* a, const void* b) -> bool {
			return *(T*)(a) == *(T*)(b);
		};

		return *this;
	}

	// Indicates that this is an enum. I'm not sure if this could be done automatically.
	template<typename T>
	TypeDesc& thisIsEnum()
	{
		enumUnderlayingType = QuickTypeId< typename std::underlying_type<T>::type >::value;
		return *this;
	}

	// Indicates that this is a std::vector. The function retrieves the underlying type and stores it.
	// And stores some function points in order to be able to call some custom methods on it.
	// The reflecton system doesn't rely on this to work.
	template<typename T>
	TypeDesc& thisIsStdVector()
	{
		stdVectorUnderlayingType = QuickTypeId< T::value_type >::value;

		stdVectorSize = [](const void* vector) -> size_t
		{
			return (*(T*)(vector)).size();
		};

		stdVectorResize = [](void* vector, size_t size) -> void
		{
			(*(T*)(vector)).resize(size);
		};

		stdVectorGetElement = [](void* vector, size_t index) -> void*
		{
			return &(*(T*)(vector))[index];
		};

		stdVectorGetElementConst = [](const void* vector, size_t index) -> const void*
		{
			return &(*(T*)(vector))[index];
		};

		return *this;
	}

	// This function depends on g_typeRegister, which is defined just below this class.
	// CAUTION: This currently doesn't work with multiple inheritance. It isn't hard to add it, I just never needed that feature.
	template<typename T>
	TypeDesc& inherits();

	// Registers a new data member to the struct.
	template<typename T, typename M>
	TypeDesc& member(const char* const name, M T::*memberPtr, unsigned const flags = 0)
	{
		MemberFieldDesc mfd;
		mfd.name = name;
		mfd.quickTypeId = QuickTypeId<M>::value;
		mfd.byteOffset = my_offsetof(memberPtr);
		mfd.sizeBytes = sizeof(M);
		mfd.flags = flags;

		members.push_back(mfd);
		return *this;
	}

	// Finds data members by a member-pointer.
	template<typename T, typename M>
	const MemberFieldDesc* findMember(M T::*memberPtr) const
	{
		int const byteOffset = my_offsetof(memberPtr);
		for(int t = 0; t < members.size(); ++t)
		{
			if(members[t].byteOffset == byteOffset)
			{
				assert(members[t].quickTypeId == QuickTypeId<M>::value);
				return &members[t];
			}
		}

		// Should never happen.
		assert(false);
		return NULL;
	}

	// Finds data members by name.
	const MemberFieldDesc* findMemberByName(const char* const memberName) const
	{
		for(int t = 0; t < (int)members.size(); ++t)
		{
			if(strcmp(members[t].name, memberName) == 0)
			{
				return &members[t];
			}
		}	

		// Should never happen.
		assert(false);
		return NULL;
	}
};

// A commonly used strcture. We do a fwddecl to potentially reduce the compile times.
extern template class std::map<QuickTypeIdType, TypeDesc>;

//--------------------------------------------------------------------------
// struct TypeRegister
//
// This is the class that holds the desciptons for every registered type.
//--------------------------------------------------------------------------
struct TypeRegister
{
	using MapTypes = std::map<QuickTypeIdType, TypeDesc>;

	template<typename T>
	TypeDesc& registerType(const char* const name)
	{
#ifdef DEBUG
		MapTypes::iterator itr = m_registeredTypes.find(QuickTypeId<T>::value);
		if(itr != std::end(m_registeredTypes)) {
			// Type is already registered this may not be intended.
			assert(false);
		}
#endif

		TypeDesc& retval = m_registeredTypes[QuickTypeId<T>::value];
		retval = TypeDesc::create<T>(name);

		return retval;
	}

	TypeDesc* find(QuickTypeIdType const quickTypeId)
	{
		MapTypes::iterator itr = m_registeredTypes.find(quickTypeId);
		if(itr == std::end(m_registeredTypes)) {
			return NULL;
		}
		
		return &itr->second;
	}

	const TypeDesc* find(QuickTypeIdType const quickTypeId) const
	{
		MapTypes::const_iterator itr = m_registeredTypes.find(quickTypeId);
		if(itr == std::end(m_registeredTypes)) {
			return NULL;
		}
		
		return &itr->second;
	}

	template<typename T>
	const TypeDesc* find() const
	{
		return find(QuickTypeId<T>::value);
	}

	TypeDesc* findByName(const char* const name)
	{
		for(MapTypes::iterator itr = m_registeredTypes.begin(); itr != m_registeredTypes.end(); ++itr)
		{
			if(strcmp(itr->second.name, name) == 0)
			{
				return &itr->second;
			}
		}
		
		return nullptr;
	}

	// Searches for a member of the specified type.
	template<typename T, typename M>
	const MemberFieldDesc* findMember(M T::*memberPtr) const
	{
		const TypeDesc* const typeDesc = find<T>();
		if(typeDesc)
		{
			return typeDesc->findMember(memberPtr);
		}

		return nullptr;
	}

	MapTypes m_registeredTypes;
};

// This is the variable that hold the reflected data. Usually you need only one per
// application, but you could use more if you want.
// I prefer global variables instead of singletons(or any alternative) as they are usually simpler.
extern TypeRegister g_typeRegister;

//----------------------------------------------------------------------------
// Continuing the TypeDesc with the things that depend on TypeRegister
//----------------------------------------------------------------------------
template<typename T>
inline TypeDesc& TypeDesc::inherits()
{
	QuickTypeIdType const superclassId = QuickTypeId<T>::value;

	// TODO: Maybe check if the superclass is already registered?
	const TypeDesc* const superTypeDesc = g_typeRegister.find(superclassId);
	assert(superTypeDesc);

	// Copy the inherited values desc. The function maintains the original superclass.
	for(const MemberFieldDesc& member : superTypeDesc->members) {
		members.push_back(member);

		if(member.inheritedForm == 0) {
			members.back().inheritedForm = superclassId;
		}
	}

	superclasses.push_back(superclassId);
	return *this;
}
