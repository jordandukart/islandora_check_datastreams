<?php
/**
 * @file
 * Functions for checking datastreams via drush.
 *
 *
 *  Copyright 2017 Leiden University Library
 *
 *  This file is part of islandora_check_datastreams.
 *
 *  islandora_check_datastreams is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * Implements hook_drush_command().
 */
function islandora_check_datastreams_drush_command() {
  $items['check_datastreams'] = array(
    'description' => 'Checks the datastreams of the objects of a specific collection, book, compound or batch set.',
    'options' => array(
      'objectid' => 'The object id of a specific collection, book or compound, or the batch set id.',
      'objecttype' => 'The type of the object. Possible values are: collection, book, compound or batch.',
    ),
    'aliases' => array('cds'),
    'examples' => array(
      'drush --user=admin check_datastreams --objectid=islandora:root --objecttype=collection',
      'drush --user=admin cds --objectid=2 --objecttype=batch',
    ),
  );
  $items['missing_datastreams_report'] = array(
    'description' => 'Reports the missing datastreams of a specific type of the objects of a specific collection, book, compound or batch set.',
    'options' => array(
      'objectid' => 'The object id of a specific collection, book or compound, or the batch set id.',
      'objecttype' => 'The type of the object. Possible values are: collection, book, compound or batch.',
      'cmodel' => 'The content model of the objects of the datastreams you are interested in.',
      'dsid' => 'The datastream id of the objects you want in the report..',
    ),
    'aliases' => array('mdsr'),
    'examples' => array(
      'drush --user=admin missing_datastreams_report --objectid=islandora:root --objecttype=collection --cmodel=islandora:pageCModel --dsid=JP2',
      'drush --user=admin mdsr --objectid=2 --objecttype=batch --cmodel=islandora:sp_large_image_cmodel --dsid=TN',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_check_datastreams_check_datastreams_validate() {
  module_load_include('inc', 'islandora_check_datastreams', 'includes/utilities');

  $objectid = drush_get_option('objectid');
  $objecttype = drush_get_option('objecttype');

  if (!($objecttype === 'collection' || $objecttype === 'book' || $objecttype === 'compound' || $objecttype === 'batch')) {
    return drush_set_error("objecttype should be one of these: collection, book, compound or batch");
  }
  if ($objecttype === 'collection') {
    $collection = islandora_object_load($objectid);
    if (!($collection && in_array('islandora:collectionCModel', $collection->models))) {
      return drush_set_error("Identifier '$objectid' does not resolve to a collection");
    }
  }
  elseif ($objecttype === 'book') {
    $book = islandora_object_load($objectid);
    if (!($book && in_array('islandora:bookCModel', $book->models))) {
      return drush_set_error("Identifier '$objectid' does not resolve to a book");
    }
  }
  elseif ($objecttype === 'compound') {
    $compound = islandora_object_load($objectid);
    if (!($compound && in_array('islandora:compoundCModel', $compound->models))) {
      return drush_set_error("Identifier '$objectid' does not resolve to a compound");
    }
  }
  elseif ($objecttype === 'batch') {
    module_load_include('inc', 'islandora_batch', 'includes/db');
    $itemids = islandora_batch_get_queued_items_for_set($objectid);
    if (!($itemids && count($itemids) > 0)) {
      return drush_set_error("Identifier '$objectid' does not resolve to a batch set");
    }
  }
  return TRUE;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_check_datastreams_check_datastreams() {
  module_load_include('inc', 'islandora_check_datastreams', 'includes/utilities');

  $objectid = drush_get_option('objectid');
  $objecttype = drush_get_option('objecttype');

  $childids = array();
  if ($objecttype === 'collection') {
    $collection = islandora_object_load($objectid);
    if ($collection && in_array('islandora:collectionCModel', $collection->models)) {
      islandora_check_datastreams_retrieve_all_child_ids($collection, $childids);
    }
  }
  elseif ($objecttype === 'book') {
    $book = islandora_object_load($objectid);
    if ($book && in_array('islandora:bookCModel', $book->models)) {
      islandora_check_datastreams_retrieve_all_child_ids($book, $childids);
    }
  }
  elseif ($objecttype === 'compound') {
    $compound = islandora_object_load($objectid);
    if ($compound && in_array('islandora:compoundCModel', $compound->models)) {
      islandora_check_datastreams_retrieve_all_child_ids($compound, $childids);
    }
  }
  elseif ($objecttype === 'batch') {
    $itemids = islandora_batch_get_queued_items_for_set($objectid);
    foreach ($itemids as $itemid) {
      $childids[] = $itemid;
    }
  }

  drush_log(t("Found !count objects, checking...", array('!count' => count($childids))), "ok");
  if (count($childids) > 0) {
    $dsidsbycmodel = islandora_check_datastreams_dsids_by_cmodel($childids);
    foreach ($dsidsbycmodel as $cmodel => $dsids) {
      $result = '';
      $vdsids = array_values($dsids);
      $total = $vdsids[0]['total'];
      $result .= format_plural($total, '1 object with content model @cmodel', '@count objects with content model @cmodel', array('@cmodel' => $cmodel)) . "\n";
      $isgood = TRUE;
      foreach ($dsids as $dsid => $counts) {
        $required = $counts['isrequired'] ? t('required') : t('optional');
        if ($counts['found'] < $counts['total']) {
          $result .= format_plural(($counts['total'] - $counts['found']), 'Missing 1 @required @type datastream', 'Missing @count @required @type datastreams', array('@required' => $required, '@type' => $dsid)) . "\n";
          $isgood = FALSE;
        }
      }
      if ($isgood) {
        $result .= t('All datastreams accounted for.') . "\n";
      }
      $result .= "\n";
      print $result;
    }
  }
  else {
    drush_log(t('No children found for @type @identifier', array('@type' => $objecttype, '@identifier' => $objectid)), "warning");
  }
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_check_datastreams_missing_datastreams_report_validate() {
  if (!drush_islandora_check_datastreams_check_datastreams_validate()) {
    return FALSE;
  }
  $cmodel = drush_get_option('cmodel');
  $dsid = drush_get_option('dsid');

  if ($cmodel === NULL || strlen($cmodel) == 0) {
    return drush_set_error("cmodel should be a valid cmodel");
  }
  if ($dsid === NULL || strlen($dsid) == 0) {
    return drush_set_error("dsid should be a valid datastream ID");
  }
  return TRUE;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_islandora_check_datastreams_missing_datastreams_report() {
  module_load_include('inc', 'islandora_check_datastreams', 'includes/utilities');

  $objectid = drush_get_option('objectid');
  $objecttype = drush_get_option('objecttype');
  $cmodel = drush_get_option('cmodel');
  $dsid = drush_get_option('dsid');
  $cmodel2dsids = array($cmodel => array($dsid));

  $csv = islandora_check_datastreams_make_csv_content($objectid, $objecttype, $cmodel2dsids);

  print $csv;
}
