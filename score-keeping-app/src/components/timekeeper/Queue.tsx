'use client';

import { DndContext, closestCenter, PointerSensor, TouchSensor, useSensor, useSensors } from '@dnd-kit/core';
import { SortableContext, verticalListSortingStrategy } from '@dnd-kit/sortable';
import { useAppDispatch, useAppSelector } from '@/store/store';
import { selectPresentCardIds } from '@/store/timekeeperSelectors';
import { moveQueueCard } from '@/store/timekeeperThunks';
import { CardRow } from './CardRow';
import type { DragEndEvent } from '@dnd-kit/core';

/** The present shooters in queue order; drag a row by its handle to reorder. */
export const Queue = () => {
  const dispatch = useAppDispatch();
  const cardIds = useAppSelector(selectPresentCardIds);
  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 5 } }),
    useSensor(TouchSensor, { activationConstraint: { delay: 150, tolerance: 8 } })
  );

  function handleDragEnd({ active, over }: DragEndEvent) {
    if (over) {
      dispatch(moveQueueCard(String(active.id), String(over.id)));
    }
  }

  return (
    <section>
      <div className="tk-section-title">Queue</div>
      <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
        <SortableContext items={cardIds} strategy={verticalListSortingStrategy}>
          <div className="tk-queue">
            {cardIds.map((cardId, index) => <CardRow key={cardId} cardId={cardId} position={index + 1} />)}
          </div>
        </SortableContext>
      </DndContext>
    </section>
  );
};
