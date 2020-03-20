#ifndef LIBGIT2_HPP
#define LIBGIT2_HPP
#include <memory>
#include <tuple>
#include <type_traits>

#include <git2.h>

namespace Git
{

namespace Detail
{

// Template-based interface to remove all pointers from a type T
template <typename T>
struct Identity
{
	using type = T;
};

template<typename T>
struct RemoveAllPointers : std::conditional_t<std::is_pointer<T>::value,
	RemoveAllPointers<std::remove_pointer_t<T>>, Identity<T>>
{};

template<typename T>
using RemoveAllPointers_t = typename RemoveAllPointers<T>::type;

// Template-based interface to query argument Ith type from a function pointer
template<std::size_t I, typename Sig>
struct GetArg;

template<std::size_t I, typename Ret, typename... Args>
struct GetArg<I, Ret(*)(Args...)>
{
	using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
};

template<std::size_t I, typename Sig>
using GetArg_t = typename GetArg<I, Sig>::type;

// Template-based interface to deduce a libgit object destructor from T
template<typename T>
struct DtorType;

template<>
struct DtorType<git_commit>
{
	using type = void(&)(git_commit*);
	static constexpr type value = git_commit_free;
};

template<>
struct DtorType<git_diff>
{
	using type = void(&)(git_diff*);
	static constexpr type value = git_diff_free;
};

template<>
struct DtorType<git_index>
{
	using type = void(&)(git_index*);
	static constexpr type value = git_index_free;
};

template<>
struct DtorType<git_object>
{
	using type = void(&)(git_object*);
	static constexpr type value = git_object_free;
};

template<>
struct DtorType<git_remote>
{
	using type = void(&)(git_remote*);
	static constexpr type value = git_remote_free;
};

template<>
struct DtorType<git_repository>
{
	using type = void(&)(git_repository*);
	static constexpr type value = git_repository_free;
};

template<>
struct DtorType<git_tree>
{
	using type = void(&)(git_tree*);
	static constexpr type value = git_tree_free;
};

template<typename T>
using DtorType_t = typename DtorType<T>::type;

template<typename T>
constexpr DtorType_t<T> DtorType_v = DtorType<T>::value;

// Template-based interface to deduce a git_otype enum value from T
template<typename T>
struct TypeEnum;

template<>
struct TypeEnum<git_tree> : std::integral_constant<git_otype, GIT_OBJ_TREE>
{};

template<typename T>
constexpr git_otype TypeEnum_v = TypeEnum<T>::value;

// libgit generic object wrapped on a std::unique_ptr
using UniqueObjPtr = std::unique_ptr<git_object, DtorType_t<git_object>>;

} // namespace Detail

constexpr const char* ESTR_GIT = "Git: {}/{} -> {:s}";

// Check error value and throw in case there is an error
inline void Check(int error)
{
	if(error >= 0)
		return;
	throw std::runtime_error(giterr_last()->message);
}

// Helper function to create RAII-managed objects for libgit C objects
template<typename Ctor,
	typename T = Detail::RemoveAllPointers_t<Detail::GetArg_t<0, Ctor>>,
	typename... Args>
decltype(auto) MakeUnique(Ctor ctor, Args&& ...args)
{
	T* obj;
	Check(ctor(&obj, std::forward<Args>(args)...));
	using namespace Detail;
	return std::unique_ptr<T, DtorType_t<T>>(std::move(obj), DtorType_v<T>);
}

// Helper function to peel a generic object into a specific libgit type
template<typename T, git_otype BT = Detail::TypeEnum_v<T>>
decltype(auto) Peel(Detail::UniqueObjPtr objPtr)
{
	T* obj;
	Check(git_object_peel(reinterpret_cast<git_object**>(&obj), objPtr.get(), BT));
	using namespace Detail;
	return std::unique_ptr<T, DtorType_t<T>>(std::move(obj), DtorType_v<T>);
}

} // namespace Git

#endif // LIBGIT2_HPP
