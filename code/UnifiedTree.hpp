#pragma once

#include "Box.hpp"
#include "TreeStructure.hpp"
#include "util.hpp"

#include <array>
#include <bitset>
#include <cassert>
#include <iostream>
#include <vector>

template<class T, int D>
class UnifiedTree {
public:
	using Index = std::array<int, D>;

	UnifiedTree(Index sizes) {
		int total = 1;
		for(int i=D-1; i>=0; --i) {
			int s = toPow2(sizes.begin()[i]);
			size[i] = s;
			stepSize[i] = total;
			total *= 2*s;
		}
		data.resize(total);
	}

	void add(const Box<D>& box, const T& value) {
		addRec(0, 0, 0, box, value);
	}

	bool check(const Box<D>& box) const {
		return checkRec(0, 0, 0, box);
	}

	void remove(Box<D> box) {
		remove(box, [](const Index&, const T&){});
	}

	template<class V>
	void remove(Box<D> box, V&& visitor) {
		for(int i=0; i<D; ++i) if (box[i].size()==0) return;
		Index ones;
		for(int i=0; i<D; ++i) ones[i]=1;
		removeRec(ones,0,0,box, visitor);
		postRemove(ones,0,0,box);
	}

	Index getSize() const { return size; }

	Box<D> boxForIndex(const Index& index) const {
		Box<D> box;
		for(int i=0; i<D; ++i) box[i] = rangeForIndex(i, index[i]);
		return box;
	}

	Range rangeForIndex(int axis, int index) const {
		return TreeStructure{2*size[axis]}.indexToRange(index);
	}

private:
	struct Item {
		std::bitset<1<<D> hasData;
		T data;
	};
	using Mask = unsigned;

	void addRec(int index, int axis, Mask covered, const Box<D>& box, const T& value) {
		if (axis == D) {
			Item& x = data[index];
			if (covered == ALL_MASK && !x.hasData[ALL_MASK]) {
				assignItem(index, value);
			} else {
				for(Mask i=0; i<1<<D; ++i) {
					if (i == (i & covered)) {
						x.hasData.set(i);
					}
				}
			}
			return;
		}
		int s = size[axis];
		int step = stepSize[axis];
		Range range = box[axis];
		if (range.size()==0) return;
		int a,b,ap,bp;
		for(a=s+range.from, b=s+range.to-1, ap=a, bp=b; a<=b; a/=2, b/=2, ap/=2, bp/=2) {
			if (a != ap) {
				addRec(index + step*ap, axis+1, covered, box, value);
			}
			if (b != bp) {
				addRec(index + step*bp, axis+1, covered, box, value);
			}
			if (a&1) {
				addRec(index + step*a++, axis+1, covered | (1U << axis), box, value);
			}
			if (!(b&1)) {
				addRec(index + step*b--, axis+1, covered | (1U << axis), box, value);
			}
		}
		for(; ap > 0; ap/=2, bp/=2) {
			addRec(index + step*ap, axis+1, covered, box, value);
			if (ap != bp) {
				addRec(index + step*bp, axis+1, covered, box, value);
			}
		}
	}

	void assignItem(int index, const T& item) {
		if (data[index].hasData[ALL_MASK]) return;
		data[index].data = item;
		data[index].hasData.set();
	}

	bool checkRec(int index, int axis, Mask covered, const Box<D>& box) const {
		if (axis == D) {
			const Item& x = data[index];
			return x.hasData[covered ^ ALL_MASK];
		}
		int s = size[axis];
		int step = stepSize[axis];
		Range range = box[axis];
		if (range.size()==0) return false;
		int a,b,ap,bp;
		for(a=s+range.from, b=s+range.to-1, ap=a, bp=b; a<=b; a/=2, b/=2, ap/=2, bp/=2) {
			if (a != ap && checkRec(index + step*ap, axis+1, covered, box)) return true;
			if (b != bp && ap!=bp && checkRec(index + step*bp, axis+1, covered, box)) return true;
			if ((a&1) && checkRec(index + step*a++, axis+1, covered | (1U << axis), box)) return true;
			if (!(b&1) && checkRec(index + step*b--, axis+1, covered | (1U << axis), box)) return true;
		}
		for(; ap > 0; ap/=2, bp/=2) {
			if (checkRec(index + step*ap, axis+1, covered, box)) return true;
			if (ap != bp && checkRec(index + step*bp, axis+1, covered, box)) return true;
		}
		return false;
	}

	template<class V>
	void removeRec(Index index, int axis, Mask covered, const Box<D>& box, V&& visitor) {
		if (axis == D) {
			int totalIndex = computeIndex(index);
			Item& x = data[totalIndex];
			if (covered != ALL_MASK && x.hasData[ALL_MASK]) {
				int splitAxis = 0;
				while(1 & (covered >> splitAxis)) ++splitAxis;
				int step = stepSize[splitAxis];
				int i = index[splitAxis];
				int baseIndex = totalIndex - step * i;
				assignItem(baseIndex + step * (2*i), x.data);
				assignItem(baseIndex + step * (2*i+1), x.data);
			}
			clearSubtree(0, index, 0, covered, visitor);
			return;
		}
		int i = index[axis];
		Range range = rangeForIndex(axis, i);
		if (!box[axis].intersects(range)) return;
		if (box[axis].contains(range)) {
			removeRec(index, axis+1, covered | (1U << axis), box, visitor);
		} else {
			removeRec(index, axis+1, covered, box, visitor);
			index[axis] = 2*i;
			removeRec(index, axis, covered, box, visitor);
			index[axis] = 2*i+1;
			removeRec(index, axis, covered, box, visitor);
		}
	}

	template<class V>
	bool clearSubtree(int totalIndex, Index index, int axis, Mask covered, V&& visitor) {
		while(axis<D && !(1&(covered>>axis))) ++axis;
		if (axis == D) {
			Item& t = data[totalIndex];
			if (t.hasData[ALL_MASK]) {
				visitor(index, t.data);
			}
			bool res = t.hasData[0];
			for(Mask i=0; i<1<<D; ++i) {
				if ((i | covered) == ALL_MASK) {
					t.hasData.reset(i);
				}
			}
			return res;
		}
		int step = stepSize[axis];
		int i = index[axis];
		bool res = clearSubtree(totalIndex + step * i, index, axis+1, covered, visitor);
		if (!res) return false;
		if (i < size[axis]) {
			index[axis] = 2*i;
			clearSubtree(totalIndex, index, axis, covered, visitor);
			index[axis] = 2*i+1;
			clearSubtree(totalIndex, index, axis, covered, visitor);
		}
		return true;
	}

	void postRemove(Index index, int axis, Mask covered, const Box<D>& box) {
		if (axis == D) {
			int totalIndex = computeIndex(index);
			Item& t = data[totalIndex];
			t.hasData.reset();
			for(int d=0; d<D; ++d) {
				if (index[d] >= size[d]) continue;
				int x = index[d];
				int step = stepSize[d];
				int baseIndex = totalIndex - step*x;
				const auto& a = data[baseIndex + step*(2*x)];
				const auto& b = data[baseIndex + step*(2*x+1)];
				for(Mask i=0; i<1<<D; ++i) {
					if (!(1 & (i>>d)) && (covered | i) != ALL_MASK) {
						t.hasData[i] = t.hasData[i] | a.hasData[i] | b.hasData[i];
					}
				}
			}
			return;
		}
		int s = size[axis];
		Range range = box[axis];
		int a,b,ap,bp;
		for(a=s+range.from, b=s+range.to-1, ap=a, bp=b; a<=b; a/=2, b/=2, ap/=2, bp/=2) {
			if (a != ap) {
				postRemove(withIndex(index, axis, ap), axis+1, covered, box);
			}
			if (b != bp) {
				postRemove(withIndex(index, axis, bp), axis+1, covered, box);
			}
			if (a&1) {
				postRemove(withIndex(index, axis, a++), axis+1, covered | (1U<<axis), box);
			}
			if (!(b&1)) {
				postRemove(withIndex(index, axis, b--), axis+1, covered | (1U<<axis), box);
			}
		}
		for(; ap > 0; ap/=2, bp/=2) {
			postRemove(withIndex(index, axis, ap), axis+1, covered, box);
			if (ap != bp) {
				postRemove(withIndex(index, axis, bp), axis+1, covered, box);
			}
		}
	}

	static Index& withIndex(Index& index, int axis, int x) {
		index[axis] = x;
		return index;
	}

	int computeIndex(const Index& index) const {
		int r=0;
		for(int i=0; i<D; ++i) r += stepSize[i] * index[i];
		return r;
	}

	static constexpr Mask ALL_MASK = (1U<<D)-1;

	Index size = {};
	Index stepSize = {};
	std::vector<Item> data;
};
