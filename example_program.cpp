#include <iostream>
#include <string>
#include "TypeRegister.h"

enum MyEnum : int
{
	myEnum_value0,
	myEnum_value1,
};

struct quaternion
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	float w = 1.f;
};

struct Data
{
	int x;
	char y;
	float z;
	MyEnum myEnumValue = myEnum_value0;
};

struct DataWithBaseClass : public Data
{
	std::string str = "hello!";
	quaternion quat;
};

int main()
{
#define REGISTER_TYPE(type) g_typeRegister.registerType<type>(#type)
#define MEMBER(TStruct, TMember) .member(#TMember, &TStruct::TMember)
#define MEMBER_FLG(TStruct, TMember, Flags) .member(#TMember, &TStruct::TMember, Flags)

	// These could be automatically embedded in the TypeRegister but for simplicity they are registered here.
	g_typeRegister.registerType<bool>("bool").constructable<bool>().copyable<bool>().compareable<bool>();
	g_typeRegister.registerType<char>("char").constructable<char>().copyable<char>().compareable<char>();
	g_typeRegister.registerType<int>("int").constructable<int>().copyable<int>().compareable<int>();
	g_typeRegister.registerType<unsigned>("unsigned").constructable<unsigned>().copyable<unsigned>().compareable<unsigned>();
	g_typeRegister.registerType<float>("float").constructable<float>().copyable<float>().compareable<float>();
	g_typeRegister.registerType<double>("double").constructable<double>().copyable<double>().compareable<double>();

	// Register a few custom types.
	REGISTER_TYPE(std::string)
		.constructable<std::string>()
		.compareable<std::string>()
		.copyable<std::string>();

	REGISTER_TYPE(MyEnum);

	REGISTER_TYPE(quaternion)
		.constructable<quaternion>().copyable<quaternion>()
		MEMBER(quaternion, x) 
		MEMBER(quaternion, y)
		MEMBER(quaternion, z)
		MEMBER(quaternion, w);

	REGISTER_TYPE(Data)
		.constructable<Data>().copyable<Data>()
		MEMBER(Data, x) 
		MEMBER(Data, y)
		MEMBER(Data, z)
		MEMBER(Data, myEnumValue);

	REGISTER_TYPE(DataWithBaseClass)
		.inherits<Data>()
		.constructable<DataWithBaseClass>().copyable<DataWithBaseClass>()
		MEMBER(DataWithBaseClass, str) 
		MEMBER(DataWithBaseClass, quat);


	// And now just use the reflection I guess!.
	for(const auto& itrTypeDesc : g_typeRegister.m_registeredTypes)
	{
		printf("Reflection for type '%s' with id %p:\n", itrTypeDesc.second.name, itrTypeDesc.first);
		printf("\tType size is %d bytes.\n", itrTypeDesc.second.sizeBytes);

		// Check if this is an enum.
		if(itrTypeDesc.second.enumUnderlayingType != nullptr)
		{
			printf("\tThe type is an enum with underlying type %s\n", g_typeRegister.find(itrTypeDesc.second.enumUnderlayingType)->name);
		}

		for(const MemberFieldDesc& mfd : itrTypeDesc.second.members)
		{
			printf("\t\t%s %s, byte offset in the struct is %d\n", g_typeRegister.find(mfd.quickTypeId)->name, mfd.name, mfd.byteOffset);
		}
	}

	return 0;
}

