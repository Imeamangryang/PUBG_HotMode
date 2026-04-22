#pragma once

#include "CoreMinimal.h"


namespace Ext
{
	// --- ConstructorHelpers::FObjectFinder helper functions ---

	template <typename T>
	bool SetObject(TObjectPtr<T>& Target, const TCHAR* ObjectToFind)
	{
		if (ConstructorHelpers::FObjectFinder<T> Object(ObjectToFind); Object.Succeeded())
		{
			Target = Object.Object;
			return true;
		}
		return false;
	}

	template <class T>
	TObjectPtr<T> OpenObject(const TCHAR* ObjectToFind, const TObjectPtr<T>& FailureValue = nullptr)
	{
		if (ConstructorHelpers::FObjectFinder<T> Object(ObjectToFind); Object.Succeeded())
		{
			return Object.Object;
		}
		return FailureValue;
	}

	template <class T>
	TObjectPtr<T> OpenObjectChecked(const TCHAR* ObjectToFind)
	{
		ConstructorHelpers::FObjectFinder<T> Object(ObjectToFind);
		check(Object.Succeeded());
		return Object.Object;
	}


	// --- ConstructorHelpers::FClassFinder helper functions ---

	template <typename T>
	bool SetClass(TSubclassOf<T>& Target, const TCHAR* ClassToFind)
	{
		if (ConstructorHelpers::FClassFinder<T> Class(ClassToFind); Class.Succeeded())
		{
			Target = Class.Class;
			return true;
		}
		return false;
	}

	template <class T>
	TSubclassOf<T> OpenClass(const TCHAR* ClassToFind, const TSubclassOf<T>& FailureValue = nullptr)
	{
		if (ConstructorHelpers::FClassFinder<T> Class(ClassToFind); Class.Succeeded())
		{
			return Class.Class;
		}
		return FailureValue;
	}

	template <class T>
	TSubclassOf<T> OpenClassChecked(const TCHAR* ClassToFind)
	{
		ConstructorHelpers::FClassFinder<T> Class(ClassToFind);
		check(Class.Succeeded());
		return Class.Class;
	}


	// Helper class for UObject
	class Bind final
	{
		TObjectPtr<UObject> Owner;

	public:
		Bind(const TObjectPtr<UObject>& Owner) : Owner(Owner)
		{
		}

		// CreateDefaultSubobject helper function
		template <class T>
		void CreateDefaultSubobject(TObjectPtr<T>& Target, FName ObjectName) const
		{
			Target = Owner->CreateDefaultSubobject<T>(ObjectName);
		}

		// NewObject helper function
		template <class T>
		void NewObject(TObjectPtr<T>& Target, FName ObjectName) const
		{
			Target = NewObject<T>(Owner, ObjectName);
		}
	};
}

#define EXT_CREATE_DEFAULT_SUBOBJECT_2_ARGS(Target, ObjectName) \
{ \
	Ext::Bind(this).CreateDefaultSubobject(Target, ObjectName); \
}
#define EXT_CREATE_DEFAULT_SUBOBJECT_1_ARGS(Target) \
	EXT_CREATE_DEFAULT_SUBOBJECT_2_ARGS(Target, TEXT(#Target))
#define EXT_CREATE_DEFAULT_SUBOBJECT__MACRO_SELECTOR(x, _1, _2, ...) _2
/**
 * @brief Wrapper macro of @c UObject::CreateDefaultSubobject .
 * @note Must be called inside a UObject constructor where @c this is valid.
 * @param Target      The TObjectPtr member variable to receive the created subobject.
 *                    Its template type is used to deduce the subobject class.
 * @param ObjectName  (Optional) The FName used to identify the subobject.
 *                    If omitted, the variable name of @p Target is used as the FName.
 * @code 
 *   EXT_CREATE_DEFAULT_SUBOBJECT(MeshComp);
 *   EXT_CREATE_DEFAULT_SUBOBJECT(MeshComp, TEXT("MyCustomName"));
 * @endcode
 */
#define EXT_CREATE_DEFAULT_SUBOBJECT(...) \
	EXT_CREATE_DEFAULT_SUBOBJECT__MACRO_SELECTOR(__VA_ARGS__, \
	EXT_CREATE_DEFAULT_SUBOBJECT_2_ARGS, \
	EXT_CREATE_DEFAULT_SUBOBJECT_1_ARGS)(__VA_ARGS__)


#define EXT_NEW_OBJECT_3_ARGS(Target, Owner, ObjectName) \
{ \
	Ext::Bind(Owner).NewObject(Target, ObjectName); \
}
#define EXT_NEW_OBJECT_2_ARGS(Target, Owner) \
	EXT_NEW_OBJECT_3_ARGS(Target, Owner, TEXT(#Target))
#define EXT_NEW_OBJECT_1_ARGS(Target) \
	EXT_NEW_OBJECT_2_ARGS(Target, this)
#define EXT_NEW_OBJECT__MACRO_SELECTOR(x, _1, _2, _3, ...) _3
/**
 * @brief Wrapper macro of @c NewObject .
 * @note Must be called where @c this is valid, and @p Owner must be a valid UObject pointer.
 * @param Target      The TObjectPtr variable to receive the created object.
 *                    Its template type is used to deduce the object class.
 * @param Owner       (Optional) The UObject that will own the created object.
 *                    If omitted, @c this is used as the owner.
 * @param ObjectName  (Optional) The FName used to identify the created object.
 *                    If omitted, the variable name of @p Target is used as the FName.
 * @code 
 *   EXT_NEW_OBJECT(MyObject);
 *   EXT_NEW_OBJECT(MyObject, Owner);
 *   EXT_NEW_OBJECT(MyObject, Owner, TEXT("MyCustomName"));
 * @endcode
 */
#define EXT_NEW_OBJECT(...) \
	EXT_NEW_OBJECT__MACRO_SELECTOR(__VA_ARGS__, \
	EXT_NEW_OBJECT_3_ARGS, \
	EXT_NEW_OBJECT_2_ARGS, \
	EXT_NEW_OBJECT_1_ARGS)(__VA_ARGS__)
