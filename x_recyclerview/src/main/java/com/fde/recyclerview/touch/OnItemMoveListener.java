/*
 * Copyright 2016 Yan Zhenjie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.fde.recyclerview.touch;

import androidx.recyclerview.widget.RecyclerView;

public interface OnItemMoveListener {

    /**
     * When drag and drop the callback.
     *
     * @param srcHolder src.
     * @param targetHolder target.
     *
     * @return To deal with the returns true, false otherwise.
     */
    boolean onItemMove(RecyclerView.ViewHolder srcHolder, RecyclerView.ViewHolder targetHolder);

    /**
     * When items should be removed when the callback.
     *
     * @param srcHolder src.
     */
    void onItemDismiss(RecyclerView.ViewHolder srcHolder);

}