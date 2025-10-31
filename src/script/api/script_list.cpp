/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_list.cpp Implementation of ScriptList. */

#include "../../stdafx.h"
#include <ranges>
#include "script_list.hpp"
#include "../../debug.h"
#include "../../script/squirrel.hpp"

#include "../../safeguards.h"

/**
 * Base class for any ScriptList sorter.
 */
class ScriptListSorter {
protected:
	ScriptList *list;       ///< The list that's being sorted.
	bool has_no_more_items; ///< Whether we have more items to iterate over.
	SQInteger item_next;    ///< The next item we will show.

public:
	/**
	 * Virtual dtor, needed to mute warnings.
	 */
	virtual ~ScriptListSorter() = default;

	/**
	 * Get the first item of the sorter.
	 */
	virtual SQInteger Begin() = 0;

	/**
	 * Stop iterating a sorter.
	 */
	virtual void End() = 0;

	/**
	 * Get the next item of the sorter.
	 */
	virtual SQInteger Next() = 0;

	/**
	 * See if the sorter has reached the end.
	 */
	bool IsEnd()
	{
		return this->list->items.empty() || this->has_no_more_items;
	}

	/**
	 * Callback from the list if an item gets removed.
	 */
	virtual void Invalidate(std::optional<SQInteger> item = {}) = 0;

	virtual void FixIterator() = 0;

	/**
	 * Attach the sorter to a new list. This assumes the content of the old list has been moved to
	 * the new list, too, so that we don't have to invalidate any iterators. Note that std::swap
	 * doesn't invalidate iterators on lists and maps, so that should be safe.
	 * @param target New list to attach to.
	 */
	virtual void Retarget(ScriptList *new_list)
	{
		this->list = new_list;
	}
};

class ScriptListSorterIndexed : public ScriptListSorter {
protected:
	using IndexType = uint32_t;

	std::vector<IndexType> indexes;
	IndexType cur_index;
	SQInteger value_next;

	ScriptListSorterIndexed(ScriptList *list)
	{
		this->list = list;
		this->ScriptListSorterIndexed::End();
	}

	virtual void SortIndexes() = 0;

	inline bool IsCurrentIndexValid() const { return this->cur_index < std::size(this->indexes); }

	void SetNextItem()
	{
		if (!this->IsCurrentIndexValid()) return;

		IndexType index = this->indexes[this->cur_index];
		this->item_next = this->list->items[index].first;
		this->value_next = this->list->items[index].second;
	}

	/**
	 * Fill the index table and sort it.
	 */
	void PrepareIndexes()
	{
		this->indexes.resize(std::size(this->list->items));
		std::iota(std::begin(this->indexes), std::end(this->indexes), 0);
		this->SortIndexes();
	}

public:
	SQInteger Begin() override
	{
		this->indexes.clear();

		if (this->list->items.empty()) return 0;
		this->has_no_more_items = false;

		this->PrepareIndexes();

		this->cur_index = 0;
		this->SetNextItem();

		SQInteger item_current = this->item_next;
		this->FindNext();
		return item_current;
	}

	void End() override
	{
		this->indexes.clear();
		this->has_no_more_items = true;
		this->item_next = 0;
	}

	void Invalidate(std::optional<SQInteger> item) override
	{
		if (this->IsEnd()) return;

		/* If the current item is changed, move on to the next. */
		if (item.has_value() && *item == this->item_next) {
			++this->cur_index;

			/* Check if we moved past the end of the list. */
			if (!this->IsCurrentIndexValid()) {
				this->End();
				return;
			}

			this->SetNextItem();
		}

		this->PrepareIndexes();
		this->FixIterator();

		if (!this->IsCurrentIndexValid()) {
			this->End();
			return;
		}

		this->SetNextItem();
	}

	/**
	 * Find the next item, and store that information.
	 */
	void FindNext()
	{
		if (!this->IsCurrentIndexValid()) {
			this->has_no_more_items = true;
			return;
		}

		++this->cur_index;
		this->SetNextItem();
	}

	SQInteger Next() override
	{
		if (this->IsEnd()) {
			this->item_next = 0;
			return 0;
		}

		SQInteger item_current = this->item_next;
		this->FindNext();
		return item_current;
	}
};

/**
 * Sort by value, ascending.
 */
class ScriptListSorterValueAscending : public ScriptListSorterIndexed {
public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueAscending(ScriptList *list) : ScriptListSorterIndexed(list) { }

private:
	void SortIndexes() override
	{
		std::ranges::sort(this->indexes, [this](const IndexType lhs, const IndexType rhs) {
			const auto &p1 = this->list->items[lhs];
			const auto &p2 = this->list->items[rhs];
			return std::tie(p1.second, p1.first) < std::tie(p2.second, p2.first);
		});
	}

	void FixIterator() override
	{
		auto it = std::ranges::lower_bound(this->indexes, this->value_next, std::less{}, [this](const IndexType &index) { return this->list->items[index].second; });
		if (it != std::end(this->indexes)) {
			it = std::ranges::lower_bound(it, std::end(this->indexes), this->item_next, std::less{}, [this](const IndexType &index) { return this->list->items[index].first; });
			if (it != std::end(this->indexes)) {
				this->cur_index = static_cast<IndexType>(std::distance(std::begin(this->indexes), it));
			}
		}
	}
};

/**
 * Sort by value, descending.
 */
class ScriptListSorterValueDescending : public ScriptListSorterIndexed {
public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterValueDescending(ScriptList *list) : ScriptListSorterIndexed(list) { }

private:
	void SortIndexes() override
	{
		std::ranges::sort(this->indexes, [this](const IndexType lhs, const IndexType rhs) {
			const auto &p1 = this->list->items[lhs];
			const auto &p2 = this->list->items[rhs];
			return std::tie(p1.second, p1.first) > std::tie(p2.second, p2.first);
		});
	}

	void FixIterator() override
	{
		auto it = std::ranges::upper_bound(this->indexes, this->value_next, std::greater{}, [this](const IndexType &index) { return this->list->items[index].second; });
		if (it != std::end(this->indexes)) {
			it = std::ranges::upper_bound(it, std::end(this->indexes), this->item_next, std::greater{}, [this](const IndexType &index) { return this->list->items[index].first; });
			if (it != std::end(this->indexes)) {
				this->cur_index = static_cast<IndexType>(std::distance(std::begin(this->indexes), it));
			}
		}
	}
};

/**
 * Sort by item, ascending.
 */
class ScriptListSorterItemAscending : public ScriptListSorterIndexed {
public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemAscending(ScriptList *list) : ScriptListSorterIndexed(list) { }

private:
	void SortIndexes() override
	{
		/* Elements are sorted by item already, so we don't need to do anything here. */
	}

	void FixIterator() override
	{
		auto it = std::ranges::lower_bound(this->indexes, this->item_next, std::less{}, [this](const IndexType &index) { return this->list->items[index].first; });
		if (it != std::end(this->indexes)) {
			this->cur_index = static_cast<IndexType>(std::distance(std::begin(this->indexes), it));
		}
	}
};

/**
 * Sort by item, descending.
 */
class ScriptListSorterItemDescending : public ScriptListSorterIndexed {
public:
	/**
	 * Create a new sorter.
	 * @param list The list to sort.
	 */
	ScriptListSorterItemDescending(ScriptList *list) : ScriptListSorterIndexed(list) { }

private:
	void SortIndexes() override
	{
		/* Elements are sorted by item already, so we can simply reverse the list instead of sorting. */
		std::ranges::reverse(this->indexes);
	}

	void FixIterator() override
	{
		auto it = std::ranges::upper_bound(this->indexes, this->item_next, std::greater{}, [this](const IndexType &index) { return this->list->items[index].first; });
		if (it != std::end(this->indexes)) {
			this->cur_index = static_cast<IndexType>(std::distance(std::begin(this->indexes), it));
		}
	}
};


bool ScriptList::SaveObject(HSQUIRRELVM vm)
{
	sq_pushstring(vm, "List");
	sq_newarray(vm, 0);
	sq_pushinteger(vm, this->sorter_type);
	sq_arrayappend(vm, -2);
	sq_pushbool(vm, this->sort_ascending ? SQTrue : SQFalse);
	sq_arrayappend(vm, -2);
	sq_newtable(vm);
	for (const auto &[key, value] : this->items) {
		sq_pushinteger(vm, key);
		sq_pushinteger(vm, value);
		sq_rawset(vm, -3);
	}
	sq_arrayappend(vm, -2);
	return true;
}

bool ScriptList::LoadObject(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, -1) != OT_ARRAY) return false;
	sq_pushnull(vm);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_INTEGER) return false;
	SQInteger type;
	sq_getinteger(vm, -1, &type);
	sq_pop(vm, 2);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_BOOL) return false;
	SQBool order;
	sq_getbool(vm, -1, &order);
	sq_pop(vm, 2);
	if (SQ_FAILED(sq_next(vm, -2))) return false;
	if (sq_gettype(vm, -1) != OT_TABLE) return false;
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		if (sq_gettype(vm, -2) != OT_INTEGER && sq_gettype(vm, -1) != OT_INTEGER) return false;
		SQInteger key, value;
		sq_getinteger(vm, -2, &key);
		sq_getinteger(vm, -1, &value);
		this->AddItem(key, value);
		sq_pop(vm, 2);
	}
	sq_pop(vm, 3);
	if (SQ_SUCCEEDED(sq_next(vm, -2))) return false;
	sq_pop(vm, 1);
	this->Sort(static_cast<SorterType>(type), order == SQTrue);
	return true;
}

ScriptObject *ScriptList::CloneObject()
{
	ScriptList *clone = new ScriptList();
	clone->CopyList(this);
	return clone;
}

void ScriptList::CopyList(const ScriptList *list)
{
	this->Sort(list->sorter_type, list->sort_ascending);
	this->items = list->items;
}

ScriptList::ScriptList()
{
	/* Default sorter */
	this->sorter         = std::make_unique<ScriptListSorterValueDescending>(this);
	this->sorter_type    = SORT_BY_VALUE;
	this->sort_ascending = false;
	this->initialized    = false;
	this->modifications  = 0;
}

ScriptList::~ScriptList()
{
}

bool ScriptList::HasItem(SQInteger item)
{
	auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
	return it != std::end(this->items) && it->first == item;
}

void ScriptList::Clear()
{
	this->modifications++;

	this->items.clear();
	this->sorter->End();
}

void ScriptList::AddItem(SQInteger item, SQInteger value)
{
	this->modifications++;

	auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
	if (it == std::end(this->items) || it->first != item) {
		it = this->items.emplace(it, item, value);
		this->sorter->Invalidate();
	}
}

void ScriptList::RemoveItem(SQInteger item)
{
	this->modifications++;

	auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
	if (it == std::end(this->items) || it->first != item) return;

	it = this->items.erase(it);
	this->sorter->Invalidate();
}

SQInteger ScriptList::Begin()
{
	this->initialized = true;
	return this->sorter->Begin();
}

SQInteger ScriptList::Next()
{
	if (!this->initialized) {
		Debug(script, 0, "Next() is invalid as Begin() is never called");
		return 0;
	}
	return this->sorter->Next();
}

bool ScriptList::IsEmpty()
{
	return this->items.empty();
}

bool ScriptList::IsEnd()
{
	if (!this->initialized) {
		Debug(script, 0, "IsEnd() is invalid as Begin() is never called");
		return true;
	}
	return this->sorter->IsEnd();
}

SQInteger ScriptList::Count()
{
	return this->items.size();
}

SQInteger ScriptList::GetValue(SQInteger item)
{
	auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
	return it == std::end(this->items) || it->first != item ? 0 : it->second;
}

bool ScriptList::SetValue(SQInteger item, SQInteger value)
{
	this->modifications++;

	auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
	if (it == std::end(this->items) || it->first != item) return false;

	SQInteger value_old = it->second;
	if (value_old == value) return true;

	it->second = value;
	this->sorter->Invalidate(item);
	return true;
}

void ScriptList::Sort(SorterType sorter, bool ascending)
{
	this->modifications++;

	if (sorter != SORT_BY_VALUE && sorter != SORT_BY_ITEM) return;
	if (sorter == this->sorter_type && ascending == this->sort_ascending) return;

	switch (sorter) {
		case SORT_BY_ITEM:
			if (ascending) {
				this->sorter = std::make_unique<ScriptListSorterItemAscending>(this);
			} else {
				this->sorter = std::make_unique<ScriptListSorterItemDescending>(this);
			}
			break;

		case SORT_BY_VALUE:
			if (ascending) {
				this->sorter = std::make_unique<ScriptListSorterValueAscending>(this);
			} else {
				this->sorter = std::make_unique<ScriptListSorterValueDescending>(this);
			}
			break;

		default: NOT_REACHED();
	}
	this->sorter_type    = sorter;
	this->sort_ascending = ascending;
	this->initialized    = false;
}

void ScriptList::AddList(ScriptList *list)
{
	if (list == this) return;

	if (this->IsEmpty()) {
		/* If this is empty, we can just take the items of the other list as is. */
		this->items = list->items;
		this->modifications++;
	} else {
		for (const auto &[item, value] : list->items) {
			auto it = std::ranges::lower_bound(this->items, item, std::less{}, &ElementType::first);
			if (it == std::end(this->items) || it->first != item) {
				this->items.emplace(it, item, value);
			} else {
				/* Item exists, replace existing value. */
				it->second = value;
			}
		}
		this->sorter->Invalidate();
	}
}

void ScriptList::SwapList(ScriptList *list)
{
	if (list == this) return;

	this->items.swap(list->items);
	std::swap(this->sorter, list->sorter);
	std::swap(this->sorter_type, list->sorter_type);
	std::swap(this->sort_ascending, list->sort_ascending);
	std::swap(this->initialized, list->initialized);
	std::swap(this->modifications, list->modifications);
	this->sorter->Retarget(this);
	list->sorter->Retarget(list);
}

void ScriptList::RemoveAboveValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second > value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::RemoveBelowValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second < value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::RemoveBetweenValue(SQInteger start, SQInteger end)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [start, end](const auto &pair) { return pair.second > start && pair.second < end; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::RemoveValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second == value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::RemoveTop(SQInteger count)
{
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveBottom(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	size_t num = std::min<size_t>(count, std::size(this->items));
	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE: {
			/* Actually sort by value temporarily. */
			std::ranges::sort(this->items, [](const auto &p1, const auto &p2) { return std::tie(p1.second, p1.first) < std::tie(p2.second, p2.first); });
			this->items.erase(std::begin(this->items), std::next(std::begin(this->items), num));
			/* Sort by item. */
			std::ranges::sort(this->items);
			this->sorter->Invalidate();
			break;
		}

		case SORT_BY_ITEM:
			this->items.erase(std::begin(this->items), std::next(std::begin(this->items), num));
			this->sorter->Invalidate();
			break;
	}
}

void ScriptList::RemoveBottom(SQInteger count)
{
	this->modifications++;

	if (!this->sort_ascending) {
		this->Sort(this->sorter_type, !this->sort_ascending);
		this->RemoveTop(count);
		this->Sort(this->sorter_type, !this->sort_ascending);
		return;
	}

	size_t num = std::min<size_t>(count, std::size(this->items));
	switch (this->sorter_type) {
		default: NOT_REACHED();
		case SORT_BY_VALUE:
			/* Actually sort by value temporarily. */
			std::ranges::sort(this->items, [](const auto &p1, const auto &p2) { return std::tie(p1.second, p1.first) < std::tie(p2.second, p2.first); });
			this->items.erase(std::prev(std::end(this->items), num), std::end(this->items));
			/* Sort by item. */
			std::ranges::sort(this->items);
			this->sorter->Invalidate();
			break;

		case SORT_BY_ITEM:
			this->items.erase(std::prev(std::end(this->items), num), std::end(this->items));
			this->sorter->Invalidate();
			break;
	}
}

void ScriptList::RemoveList(ScriptList *list)
{
	this->modifications++;

	if (list == this) {
		this->Clear();
	} else {
		auto remove = std::remove_if(std::begin(this->items), std::end(this->items), [list](const auto &pair)
		{
			auto it = std::ranges::lower_bound(list->items, pair.first, std::less{}, &ElementType::first);
			return it != std::end(list->items) && it->first == pair.first;
		});
		this->items.erase(remove, std::end(this->items));
		this->sorter->Invalidate();
	}
}

void ScriptList::KeepAboveValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second <= value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::KeepBelowValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second >= value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::KeepBetweenValue(SQInteger start, SQInteger end)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [start, end](const auto &pair) { return pair.second <= start || pair.second >= end; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::KeepValue(SQInteger value)
{
	this->modifications++;

	auto it = std::remove_if(std::begin(this->items), std::end(this->items), [value](const auto &pair) { return pair.second != value; });
	this->items.erase(it, std::end(this->items));
	this->sorter->Invalidate();
}

void ScriptList::KeepTop(SQInteger count)
{
	this->modifications++;

	this->RemoveBottom(this->Count() - count);
}

void ScriptList::KeepBottom(SQInteger count)
{
	this->modifications++;

	this->RemoveTop(this->Count() - count);
}

void ScriptList::KeepList(ScriptList *list)
{
	if (list == this) return;

	this->modifications++;

	ScriptList tmp;
	tmp.AddList(this);
	tmp.RemoveList(list);
	this->RemoveList(&tmp);
}

SQInteger ScriptList::_get(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	auto it = std::ranges::lower_bound(this->items, idx, std::less{}, &ElementType::first);
	if (it == std::end(this->items) || it->first != idx) return SQ_ERROR;

	sq_pushinteger(vm, it->second);
	return 1;
}

SQInteger ScriptList::_set(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) != OT_INTEGER) return SQ_ERROR;

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	/* Retrieve the return value */
	SQInteger val;
	switch (sq_gettype(vm, 3)) {
		case OT_NULL:
			this->RemoveItem(idx);
			return 0;

		case OT_BOOL: {
			SQBool v;
			sq_getbool(vm, 3, &v);
			val = v ? 1 : 0;
			break;
		}

		case OT_INTEGER:
			sq_getinteger(vm, 3, &val);
			break;

		default:
			return sq_throwerror(vm, "you can only assign integers to this list");
	}

	if (!this->HasItem(idx)) {
		this->AddItem(idx, val);
		return 0;
	}

	this->SetValue(idx, val);
	return 0;
}

SQInteger ScriptList::_nexti(HSQUIRRELVM vm)
{
	if (sq_gettype(vm, 2) == OT_NULL) {
		if (this->IsEmpty()) {
			sq_pushnull(vm);
			return 1;
		}
		sq_pushinteger(vm, this->Begin());
		return 1;
	}

	SQInteger idx;
	sq_getinteger(vm, 2, &idx);

	SQInteger val = this->Next();
	if (this->IsEnd()) {
		sq_pushnull(vm);
		return 1;
	}

	sq_pushinteger(vm, val);
	return 1;
}

SQInteger ScriptList::Valuate(HSQUIRRELVM vm)
{
	this->modifications++;

	/* The first parameter is the instance of ScriptList. */
	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1) {
		return sq_throwerror(vm, "You need to give at least a Valuator as parameter to ScriptList::Valuate");
	}

	/* Make sure the valuator function is really a function, and not any
	 * other type. It's parameter 2 for us, but for the user it's the
	 * first parameter they give. */
	SQObjectType valuator_type = sq_gettype(vm, 2);
	if (valuator_type != OT_CLOSURE && valuator_type != OT_NATIVECLOSURE) {
		return sq_throwerror(vm, "parameter 1 has an invalid type (expected function)");
	}

	/* Don't allow docommand from a Valuator, as we can't resume in
	 * mid C++-code. */
	ScriptObject::DisableDoCommandScope disabler{};

	/* Limit the total number of ops that can be consumed by a valuate operation */
	SQOpsLimiter limiter(vm, MAX_VALUATE_OPS, "valuator function");

	/* Push the function to call */
	sq_push(vm, 2);

	for (auto &[key, _] : this->items) {
		/* Check for changing of items. */
		int previous_modification_count = this->modifications;

		/* Push the root table as instance object, this is what squirrel does for meta-functions. */
		sq_pushroottable(vm);
		/* Push all arguments for the valuator function. */
		sq_pushinteger(vm, key);
		for (int i = 0; i < nparam - 1; i++) {
			sq_push(vm, i + 3);
		}

		/* Call the function. Squirrel pops all parameters and pushes the return value. */
		if (SQ_FAILED(sq_call(vm, nparam + 1, SQTrue, SQFalse))) {
			return SQ_ERROR;
		}

		/* Retrieve the return value */
		SQInteger value;
		switch (sq_gettype(vm, -1)) {
			case OT_INTEGER: {
				sq_getinteger(vm, -1, &value);
				break;
			}

			case OT_BOOL: {
				SQBool v;
				sq_getbool(vm, -1, &v);
				value = v ? 1 : 0;
				break;
			}

			default: {
				/* See below for explanation. The extra pop is the return value. */
				sq_pop(vm, nparam + 4);

				return sq_throwerror(vm, "return value of valuator is not valid (not integer/bool)");
			}
		}

		/* Was something changed? */
		if (previous_modification_count != this->modifications) {
			/* See below for explanation. The extra pop is the return value. */
			sq_pop(vm, nparam + 4);

			return sq_throwerror(vm, "modifying valuated list outside of valuator function");
		}

		this->SetValue(key, value);

		/* Pop the return value. */
		sq_poptop(vm);

		Squirrel::DecreaseOps(vm, 5);
	}
	/* Pop from the squirrel stack:
	 * 1. The root stable (as instance object).
	 * 2. The valuator function.
	 * 3. The parameters given to this function.
	 * 4. The ScriptList instance object. */
	sq_pop(vm, nparam + 3);

	return 0;
}
